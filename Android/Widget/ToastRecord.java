// 文件分布：frameworks/base/services/core/java/com/android/server/notification/toast/ToastRecord.java

/**
 * Represents a toast, a transient notification.
 */
public abstract class ToastRecord {
    public final int uid;
    public final int pid;
    public final String pkg;
    public final IBinder token;
    public final int displayId;
    public final Binder windowToken;
    protected final NotificationManagerService mNotificationManager;
    private int mDuration;

    protected ToastRecord(NotificationManagerService notificationManager, int uid, int pid,
            String pkg, IBinder token, int duration, Binder windowToken, int displayId) {
        this.mNotificationManager = notificationManager;
        this.uid = uid;
        this.pid = pid;
        this.pkg = pkg;
        this.token = token;
        this.windowToken = windowToken;
        this.displayId = displayId;
        mDuration = duration;
    }

    /**
     * This method is responsible for showing the toast represented by this object.
     *
     * @return True if it was successfully shown.
     */
    public abstract boolean show();

    /**
     * This method is responsible for hiding the toast represented by this object.
     */
    public abstract void hide();

    /**
     * Returns the duration of this toast, which can be {@link android.widget.Toast#LENGTH_SHORT}
     * or {@link android.widget.Toast#LENGTH_LONG}.
     */
    public int getDuration() {
        return mDuration;
    }

    /**
     * Updates toast duration.
     */
    public void update(int duration) {
        mDuration = duration;
    }

    /**
     * Dumps a textual representation of this object.
     */
    public void dump(PrintWriter pw, String prefix, DumpFilter filter) {
        if (filter != null && !filter.matches(pkg)) {
            return;
        }
        pw.println(prefix + this);
    }
}
