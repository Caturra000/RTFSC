// 文件分布：frameworks/base/services/core/java/com/android/server/notification/NotificationManagerService.java

public class NotificationManagerService extends SystemService {

    // ...

        @VisibleForTesting
    final IBinder mService = new INotificationManager.Stub() {
        // Toasts
        // ============================================================================

        // 在Client端中，下述参数对应于
        // token: mToken = new Binder();
        // callback: mTN = new TN(context, context.getPackageName(), mToken, mCallbacks, looper);


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

        // 1. R上新增block判断
        // 2. 入队调度
        private void enqueueToast(String pkg, IBinder token, @Nullable CharSequence text,
                @Nullable ITransientNotification callback, int duration, int displayId,
                @Nullable ITransientNotificationCallback textCallback) {
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
            // 从下面的注释可以知道，isAppRenderedToast就是判断是否为一个自定义toast
            // 会应用于boolean block的判断
            boolean isAppRenderedToast = (callback != null);
            // 自定义 / 非系统 / 位于后台 则有可能block（需要target api的判断）
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

            // 调度
            synchronized (mToastQueue) {
                int callingPid = Binder.getCallingPid();
                long callingId = Binder.clearCallingIdentity();
                try {
                    ToastRecord record;
                    int index = indexOfToastLocked(pkg, token);
                    // If it's already in the queue, we update it in place, we don't
                    // move it to the end of the queue.
                    if (index >= 0) {
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
                                if (count >= MAX_PACKAGE_NOTIFICATIONS) {
                                    Slog.e(TAG, "Package has already posted " + count
                                            + " toasts. Not showing more. Package=" + pkg);
                                    return;
                                }
                            }
                        }

                        Binder windowToken = new Binder();
                        mWindowManagerInternal.addWindowToken(windowToken, TYPE_TOAST, displayId);          // TODO
                        // 构造一个TextRecord或者CustomRecord
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
                        showNextToastLocked(); // 这个方法总是从0开始调度，因此需要判断index == 0
                    }
                } finally {
                    Binder.restoreCallingIdentity(callingId);
                }
            }
        }

        // ...

    }


    private ToastRecord getToastRecord(int uid, int pid, String packageName, IBinder token,
            @Nullable CharSequence text, @Nullable ITransientNotification callback, int duration,
            Binder windowToken, int displayId,
            @Nullable ITransientNotificationCallback textCallback) {
        if (callback == null) {
            return new TextToastRecord(this, mStatusBar, uid, pid, packageName, token, text,
                    duration, windowToken, displayId, textCallback);
        } else {
            // here
            return new CustomToastRecord(this, uid, pid, packageName, token, callback, duration,
                    windowToken, displayId);
        }
    }

    void showNextToastLocked() {
        ToastRecord record = mToastQueue.get(0);
        while (record != null) {
            if (record.show()) {
                // 扔出超时MSG，到达duration后会触发
                scheduleDurationReachedLocked(record);
                return;
            }
            // 如果失败则移除，直到某一个record成功show 
            // 或者 已经没有了，没有的话会在下一次插入时(必然index == 0)再次showNextToastLocked
            int index = mToastQueue.indexOf(record);
            if (index >= 0) {
                mToastQueue.remove(index);
            }
            record = (mToastQueue.size() > 0) ? mToastQueue.get(0) : null;
        }
    }

    private void scheduleDurationReachedLocked(ToastRecord r)
    {
        // 清空handler中的mMessages
        mHandler.removeCallbacksAndMessages(r);
        Message m = Message.obtain(mHandler, MESSAGE_DURATION_REACHED, r);
        int delay = r.getDuration() == Toast.LENGTH_LONG ? LONG_DELAY : SHORT_DELAY;
        // Accessibility users may need longer timeout duration. This api compares original delay
        // with user's preference and return longer one. It returns original delay if there's no
        // preference.
        delay = mAccessibilityManager.getRecommendedTimeoutMillis(delay,
                AccessibilityManager.FLAG_CONTENT_TEXT);
        mHandler.sendMessageDelayed(m, delay);
    }

    private WorkerHandler mHandler;

        protected class WorkerHandler extends Handler
    {
        public WorkerHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg)
        {
            switch (msg.what)
            {
                case MESSAGE_DURATION_REACHED:
                    handleDurationReached((ToastRecord) msg.obj);
                    break;
                case MESSAGE_FINISH_TOKEN_TIMEOUT:
                    handleKillTokenTimeout((ToastRecord) msg.obj);
                    break;
                case MESSAGE_SEND_RANKING_UPDATE:
                    handleSendRankingUpdate();
                    break;
                case MESSAGE_LISTENER_HINTS_CHANGED:
                    handleListenerHintsChanged(msg.arg1);
                    break;
                case MESSAGE_LISTENER_NOTIFICATION_FILTER_CHANGED:
                    handleListenerInterruptionFilterChanged(msg.arg1);
                    break;
                case MESSAGE_ON_PACKAGE_CHANGED:
                    SomeArgs args = (SomeArgs) msg.obj;
                    handleOnPackageChanged((boolean) args.arg1, args.argi1, (String[]) args.arg2,
                            (int[]) args.arg3);
                    args.recycle();
                    break;
            }
        }

        // ...
    }


}