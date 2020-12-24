
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

    // TN中
    private static class TN extends ITransientNotification.Stub {
        /**
         * schedule handleShow into the right thread
         */
        // Server端远程调用，发出SHOW msg
        @Override
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
        public void show(IBinder windowToken) {
            if (localLOGV) Log.v(TAG, "SHOW: " + this);
            mHandler.obtainMessage(SHOW, windowToken).sendToTarget();
        }

        // 构造函数中
        TN(Context context, String packageName, Binder token, List<Callback> callbacks,
                @Nullable Looper looper) {
            // ...略
            mHandler = new Handler(looper, null) {
                @Override
                public void handleMessage(Message msg) {
                    switch (msg.what) {
// TRACK BEGIN
                        case SHOW: {
                            IBinder token = (IBinder) msg.obj;
                            handleShow(token);
                            break;
                        }
// TRACK END
                        case HIDE: {
                            handleHide();
                            // Don't do this in handleHide() because it is also invoked by
                            // handleShow()
                            mNextView = null;
                            break;
                        }
                        case CANCEL: {
                            handleHide();
                            // Don't do this in handleHide() because it is also invoked by
                            // handleShow()
                            mNextView = null;
                            try {
                                getService().cancelToast(mPackageName, mToken);
                            } catch (RemoteException e) {
                            }
                            break;
                        }
                    }
                }
            };
        }

        public void handleShow(IBinder windowToken) {
            if (localLOGV) Log.v(TAG, "HANDLE SHOW: " + this + " mView=" + mView
                    + " mNextView=" + mNextView);
            // If a cancel/hide is pending - no need to show - at this point
            // the window token is already invalid and no need to do any work.
            if (mHandler.hasMessages(CANCEL) || mHandler.hasMessages(HIDE)) {
                return;
            }
            if (mView != mNextView) {
                // remove the old view if necessary
                handleHide(); // 处理mView，表示已经显示过的内容
                mView = mNextView;
                mPresenter.show(mView, mToken, windowToken, mDuration, mGravity, mX, mY,
                        mHorizontalMargin, mVerticalMargin,
                        new CallbackBinder(getCallbacks(), mHandler));
            }
        }

        public void handleHide() {
            if (localLOGV) Log.v(TAG, "HANDLE HIDE: " + this + " mView=" + mView);
            if (mView != null) {
                checkState(mView == mPresenter.getView(),
                        "Trying to hide toast view different than the last one displayed");
                mPresenter.hide(new CallbackBinder(getCallbacks(), mHandler));
                mView = null;
            }
        }



    }
}


