// 文件分布：frameworks/base/services/core/java/com/android/server/notification/toast/CustomToastRecord.java

/**
 * Represents a custom toast, a toast whose view is provided by the app.
 */
public class CustomToastRecord extends ToastRecord {
    private static final String TAG = NotificationManagerService.TAG;

    public final ITransientNotification callback;

    public CustomToastRecord(NotificationManagerService notificationManager, int uid, int pid,
            String packageName, IBinder token, ITransientNotification callback, int duration,
            Binder windowToken, int displayId) {
        super(notificationManager, uid, pid, packageName, token, duration, windowToken, displayId);
        this.callback = checkNotNull(callback);
    }

    @Override
    public boolean show() {
        if (DBG) {
            Slog.d(TAG, "Show pkg=" + pkg + " callback=" + callback);
        }
        try {
            callback.show(windowToken);
            return true;
        } catch (RemoteException e) {
            Slog.w(TAG, "Object died trying to show custom toast " + token + " in package "
                    + pkg);
            mNotificationManager.keepProcessAliveForToastIfNeeded(pid);
            return false;
        }
    }

    @Override
    public void hide() {
        try {
            callback.hide();
        } catch (RemoteException e) {
            Slog.w(TAG, "Object died trying to hide custom toast " + token + " in package "
                    + pkg);

        }
    }

    @Override
    public String toString() {
        return "CustomToastRecord{"
                + Integer.toHexString(System.identityHashCode(this))
                + " " + pid + ":" +  pkg + "/" + UserHandle.formatUid(uid)
                + " token=" + token
                + " callback=" + callback
                + " duration=" + getDuration()
                + "}";
    }
}
