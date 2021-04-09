// 文件：frameworks/base/core/java/android/util/Log.java

// TL;DR 日志的写入就是简单的用writev封装一下，使用socket与其它进程通信
// TODO logd进程

public final class Log {

    private Log() {
    }

    /**
     * Send a {@link #VERBOSE} log message.
     * @param tag Used to identify the source of a log message.  It usually identifies
     *        the class or activity where the log call occurs.
     * @param msg The message you would like logged.
     */
    public static int v(@Nullable String tag, @NonNull String msg) {
        return println_native(LOG_ID_MAIN, VERBOSE, tag, msg);
    }

    public static native int println_native(int bufID, int priority, String tag, String msg);

}