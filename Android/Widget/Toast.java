
// 文件分布：frameworks/base/core/java/android/widget/Toast.java


public class Toast {

    // ...

    private final Binder mToken;
    private final Context mContext;
    private final Handler mHandler;
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P)
    final TN mTN;
    @UnsupportedAppUsage
    int mDuration;

    /**
     * This is also passed to {@link TN} object, where it's also accessed with itself as its own
     * lock.
     */
    @GuardedBy("mCallbacks")
    private final List<Callback> mCallbacks;

    /**
     * View to be displayed, in case this is a custom toast (e.g. not created with {@link
     * #makeText(Context, int, int)} or its variants).
     */
    @Nullable
    private View mNextView;

    /**
     * Text to be shown, in case this is NOT a custom toast (e.g. created with {@link
     * #makeText(Context, int, int)} or its variants).
     */
    @Nullable
    private CharSequence mText;


    public Toast(@NonNull Context context, @Nullable Looper looper) {
        mContext = context;
        mToken = new Binder();
        looper = getLooper(looper);
        mHandler = new Handler(looper);
        mCallbacks = new ArrayList<>();
        mTN = new TN(context, context.getPackageName(), mToken, // TN获取包名
                mCallbacks, looper);
        mTN.mY = context.getResources().getDimensionPixelSize(
                com.android.internal.R.dimen.toast_y_offset);
        mTN.mGravity = context.getResources().getInteger(
                com.android.internal.R.integer.config_toastDefaultGravity);
    }

    /**
     * @param context  The context to use.  Usually your {@link android.app.Application}
     *                 or {@link android.app.Activity} object.
     */
    public static Toast makeText(Context context, CharSequence text, @Duration int duration) {
        return makeText(context, null, text, duration); // looper为null，既使用myLooper()
    }

    public static Toast makeText(@NonNull Context context, @Nullable Looper looper,
            @NonNull CharSequence text, @Duration int duration) {
        if (Compatibility.isChangeEnabled(CHANGE_TEXT_TOASTS_IN_THE_SYSTEM)) {
            Toast result = new Toast(context, looper);
            result.mText = text;
            result.mDuration = duration;
            return result;
        } else {
            Toast result = new Toast(context, looper);
            View v = ToastPresenter.getTextToastView(context, text);   // TODO
            result.mNextView = v;
            result.mDuration = duration;

            return result;
        }
    }



    public void show() {
        if (Compatibility.isChangeEnabled(CHANGE_TEXT_TOASTS_IN_THE_SYSTEM)) {
            checkState(mNextView != null || mText != null, "You must either set a text or a view");
        } else {
            if (mNextView == null) {
                throw new RuntimeException("setView must have been called");
            }
        }

        // 这里涉及到两个重要的类，NMS和TN

        // 获取NMS，用于管理所有需要显示的Toast
        INotificationManager service = getService();
        String pkg = mContext.getOpPackageName();
        // TN表示TransientNotification
        TN tn = mTN;                                                                          // TODO TN
        tn.mNextView = mNextView;
        final int displayId = mContext.getDisplayId();

        try {
            if (Compatibility.isChangeEnabled(CHANGE_TEXT_TOASTS_IN_THE_SYSTEM)) {
                if (mNextView != null) {
                    // It's a custom toast
                    service.enqueueToast(pkg, mToken, tn, mDuration, displayId);
                } else {
                    // It's a text toast
                    ITransientNotificationCallback callback =
                            new CallbackBinder(mCallbacks, mHandler);
                    service.enqueueTextToast(pkg, mToken, mText, mDuration, displayId, callback);
                }
            } else {
// TRACK BEGIN
                service.enqueueToast(pkg, mToken, tn, mDuration, displayId);
// TRACK END
            }
        } catch (RemoteException e) {
            // Empty
        }
    }

}




    // 文件分布：frameworks/base/services/core/java/com/android/server/notification/NotificationManagerService.java

    final IBinder mService = new INotificationManager.Stub() {
        @Override
        public void enqueueTextToast(String pkg, IBinder token, CharSequence text, int duration,
                int displayId, @Nullable ITransientNotificationCallback callback) {
            enqueueToast(pkg, token, text, null, duration, displayId, callback);
        }

        @Override
        public void enqueueToast(String pkg, IBinder token, ITransientNotification callback,
                int duration, int displayId) {
            enqueueToast(pkg, token, null, callback, duration, displayId, null);
        }

        private void enqueueToast(String pkg, IBinder token, @Nullable CharSequence text,
                @Nullable ITransientNotification callback, int duration, int displayId,
                @Nullable ITransientNotificationCallback textCallback) {
// SKIP BEGIN
            if (DBG) {
                Slog.i(TAG, "enqueueToast pkg=" + pkg + " token=" + token
                        + " duration=" + duration + " displayId=" + displayId);
            }

            if (pkg == null || (text == null && callback == null)
                    || (text != null && callback != null) || token == null) {
                Slog.e(TAG, "Not enqueuing toast. pkg=" + pkg + " text=" + text + " callback="
                        + " token=" + token);
                return;
            }

            final int callingUid = Binder.getCallingUid();
            final UserHandle callingUser = Binder.getCallingUserHandle();
            final boolean isSystemToast = isCallerSystemOrPhone()
                    || PackageManagerService.PLATFORM_PACKAGE_NAME.equals(pkg);
            final boolean isPackageSuspended = isPackagePaused(pkg);
            final boolean notificationsDisabledForPackage = !areNotificationsEnabledForPackage(pkg,
                    callingUid);

            final boolean appIsForeground;
            long callingIdentity = Binder.clearCallingIdentity();
            try {
                appIsForeground = mActivityManager.getUidImportance(callingUid)
                        == IMPORTANCE_FOREGROUND;
            } finally {
                Binder.restoreCallingIdentity(callingIdentity);
            }

            if (ENABLE_BLOCKED_TOASTS && !isSystemToast && ((notificationsDisabledForPackage
                    && !appIsForeground) || isPackageSuspended)) {
                Slog.e(TAG, "Suppressing toast from package " + pkg
                        + (isPackageSuspended ? " due to package suspended."
                        : " by user request."));
                return;
            }

            boolean isAppRenderedToast = (callback != null);
            if (isAppRenderedToast && !isSystemToast && !isPackageInForegroundForToast(pkg,
                    callingUid)) {
                boolean block;
                long id = Binder.clearCallingIdentity();
                try {
                    // CHANGE_BACKGROUND_CUSTOM_TOAST_BLOCK is gated on targetSdk, so block will be
                    // false for apps with targetSdk < R. For apps with targetSdk R+, text toasts
                    // are not app-rendered, so isAppRenderedToast == true means it's a custom
                    // toast.
                    block = mPlatformCompat.isChangeEnabledByPackageName(
                            CHANGE_BACKGROUND_CUSTOM_TOAST_BLOCK, pkg,
                            callingUser.getIdentifier());
                } catch (RemoteException e) {
                    // Shouldn't happen have since it's a local local
                    Slog.e(TAG, "Unexpected exception while checking block background custom toasts"
                            + " change", e);
                    block = false;
                } finally {
                    Binder.restoreCallingIdentity(id);
                }
                if (block) {
                    Slog.w(TAG, "Blocking custom toast from package " + pkg
                            + " due to package not in the foreground");
                    return;
                }
            }
// SKIP END
            synchronized (mToastQueue) {
                int callingPid = Binder.getCallingPid();
                long callingId = Binder.clearCallingIdentity();
                try {
                    ToastRecord record;
                    int index = indexOfToastLocked(pkg, token);
                    // If it's already in the queue, we update it in place, we don't
                    // move it to the end of the queue.
                    if (index >= 0) { // 已存在，则inplace更新duration
                        record = mToastQueue.get(index);
                        record.update(duration);
                    } else {
                        // Limit the number of toasts that any given package can enqueue.
                        // Prevents DOS attacks and deals with leaks.
                        int count = 0;
                        final int N = mToastQueue.size();
                        for (int i = 0; i < N; i++) {
                            final ToastRecord r = mToastQueue.get(i);
                            if (r.pkg.equals(pkg)) {
                                count++;
                                // 每一个Package都有最大限制，超过则结束
                                if (count >= MAX_PACKAGE_NOTIFICATIONS) {
                                    Slog.e(TAG, "Package has already posted " + count
                                            + " toasts. Not showing more. Package=" + pkg);
                                    return;
                                }
                            }
                        }
                        // 构造 插入
                        Binder windowToken = new Binder();
                        mWindowManagerInternal.addWindowToken(windowToken, TYPE_TOAST, displayId);
                        record = getToastRecord(callingUid, callingPid, pkg, token, text, callback,
                                duration, windowToken, displayId, textCallback);
                        mToastQueue.add(record);
                        index = mToastQueue.size() - 1;
                        keepProcessAliveForToastIfNeededLocked(callingPid);
                    }
                    // If it's at index 0, it's the current toast.  It doesn't matter if it's
                    // new or just been updated, show it.
                    // If the callback fails, this will remove it from the list, so don't
                    // assume that it's valid after this.
                    if (index == 0) {
// TRACK BEGIN
                        showNextToastLocked();
// TRACK END
                    }
                } finally {
                    Binder.restoreCallingIdentity(callingId);
                }
            }
        }

        //...

    }

    void showNextToastLocked() {
        ToastRecord record = mToastQueue.get(0);
        while (record != null) {
            if (record.show()) { // 不满足显示条件则false
                scheduleDurationReachedLocked(record);
                return;
            }
            // 已经不满足条件了，移除
            int index = mToastQueue.indexOf(record);
            if (index >= 0) {
                mToastQueue.remove(index);
            }
            record = (mToastQueue.size() > 0) ? mToastQueue.get(0) : null;
        }
    }

