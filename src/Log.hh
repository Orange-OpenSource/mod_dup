#ifndef LOG_HH_
#define LOG_HH_

//#include <cstdarg>
#include <string>

/// @brief logging utility.
/// syslog calls are wrapped
/// @see man syslog
class Log
{
public:
    /***
    *  Init the Log
    *  pFacility: the "device" which handles application messages
    */
    static void init(int pFacility);

    /// @brief Init the Log
    static void init();

    // Close Log
    static void close();

    /***
     * logs a debug with message parameters <...>
     * pMsg: the message
     */
    static void debug(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs an info with message parameters <...>
     * pMsg: the message
     */
    static void info(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs a notice with message parameters <...>
     * pMsg: the message
     */
    static void notice(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs a warning with message parameters <...>
     * pMsg: the message
     */
    static void warn(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs an error with message parameters <...>
     * pMsg: the message
     */
    static void error(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));
    /***
     * logs a crit with message parameters <...>
     * pMsg: the message
     */
    static void crit(int pCode, const char* pMsg, ...) __attribute__ ((format (printf, 2, 3)));

    static const char * stringLevel(int pLevel);

protected:
    /// @brief pointer to the only instance of this class
    static Log* gInstance;

    static int gLogLevel;

    static const char *STRDEBUG;
    static const char *STRINFO;
    static const char *STRNOTICE;
    static const char *STRWARN;
    static const char *STRERROR;
    static const char *STRCRIT;
    static const char *IDENT;

	const int mFacility;

    /***
     * Log's constructor, assignment operator, and copy constructor are declared protected
     * to ensure that users can't create local instances of the class
     */
    Log(int pFacility);
    Log(const Log&);
    Log& operator= (const Log&);

    virtual ~Log();


public:


};



#endif /* LOG_HH_ */
