/* Apparently, Boost does not have semaphores. So, we implement one.
 * Example from:
 * http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
 */

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
    /* UNIX-style OS. ------------------------------------------- */
#include <unistd.h>
#endif

#if defined(_POSIX_VERSION)
// Posix!

#include <semaphore.h>

namespace apex {

class semaphore
{
private:
    sem_t the_semaphore;
    int work_waiting;
public:
    semaphore() : work_waiting(0) { sem_init(&the_semaphore, 1, 1); }
    /*
     * This function is somewhat optimized. Because we were spending a lot of time
     * waiting for the post (it is a synchronization point across all threads), don't
     * post if there is already work on the queue.
     */
    inline void post() { if (work_waiting) return ;
        __sync_fetch_and_add(&work_waiting, 1) ;
        sem_post(&the_semaphore); }
    /*
     * When the wait is over, clear the "work_waiting" flag, even though we haven't
     * cleared the waiting profilers.
     */
    inline void wait() { sem_wait(&the_semaphore);
        __sync_fetch_and_sub(&work_waiting, work_waiting); }
};

}

#else
// Not posix, so use Boost to build a semaphore.

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

namespace apex {

class semaphore
{
private:
    boost::mutex mutex_;
    boost::condition_variable condition_;
    unsigned long count_;

public:
    semaphore()
        : count_()
    {}

    void post()
    {
        boost::mutex::scoped_lock lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void wait()
    {
        boost::mutex::scoped_lock lock(mutex_);
        while(!count_)
            condition_.wait(lock);
        --count_;
    }
};

};

#endif
// end of "No posix"
