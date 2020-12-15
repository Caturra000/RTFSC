/**
 * Base class for code that receives and handles broadcast intents sent by
 * {@link android.content.Context#sendBroadcast(Intent)}.
 *
 * <p>You can either dynamically register an instance of this class with
 * {@link Context#registerReceiver Context.registerReceiver()}
 * or statically declare an implementation with the
 * {@link android.R.styleable#AndroidManifestReceiver &lt;receiver&gt;}
 * tag in your <code>AndroidManifest.xml</code>.
 *
 * <div class="special reference">
 * <h3>Developer Guides</h3>
 * <p>For more information about using BroadcastReceiver, read the
 * <a href="{@docRoot}guide/components/broadcasts.html">Broadcasts</a> developer guide.</p></div>
 *
 */
public abstract class BroadcastReceiver {

    private PendingResult mPendingResult;


    /**
     * State for a result that is pending for a broadcast receiver.  Returned
     * by {@link BroadcastReceiver#goAsync() goAsync()}
     * while in {@link BroadcastReceiver#onReceive BroadcastReceiver.onReceive()}.
     * This allows you to return from onReceive() without having the broadcast
     * terminate; you must call {@link #finish()} once you are done with the
     * broadcast.  This allows you to process the broadcast off of the main
     * thread of your app.
     *
     * <p>Note on threading: the state inside of this class is not itself
     * thread-safe, however you can use it from any thread if you properly
     * sure that you do not have races.  Typically this means you will hand
     * the entire object to another thread, which will be solely responsible
     * for setting any results and finally calling {@link #finish()}.
     */
    public static class PendingResult {

        /**
         * Finish the broadcast.  The current result will be sent and the
         * next broadcast will proceed.
         */
        public final void finish() {
            if (mType == TYPE_COMPONENT) {
                final IActivityManager mgr = ActivityManager.getService();
                if (QueuedWork.hasPendingWork()) {
                    // If this is a broadcast component, we need to make sure any
                    // queued work is complete before telling AM we are done, so
                    // we don't have our process killed before that.  We now know
                    // there is pending work; put another piece of work at the end
                    // of the list to finish the broadcast, so we don't block this
                    // thread (which may be the main thread) to have it finished.
                    //
                    // Note that we don't need to use QueuedWork.addFinisher() with the
                    // runnable, since we know the AM is waiting for us until the
                    // executor gets to it.
                    QueuedWork.queue(new Runnable() {
                        @Override public void run() {
                            if (ActivityThread.DEBUG_BROADCAST) Slog.i(ActivityThread.TAG,
                                    "Finishing broadcast after work to component " + mToken);
                            sendFinished(mgr);
                        }
                    }, false);
                } else {
                    if (ActivityThread.DEBUG_BROADCAST) Slog.i(ActivityThread.TAG,
                            "Finishing broadcast to component " + mToken);
                    sendFinished(mgr);
                }
            } else if (mOrderedHint && mType != TYPE_UNREGISTERED) {
                if (ActivityThread.DEBUG_BROADCAST) Slog.i(ActivityThread.TAG,
                        "Finishing broadcast to " + mToken);
                final IActivityManager mgr = ActivityManager.getService();
                sendFinished(mgr);
            }
        }

    }



    /**
     * This method is called when the BroadcastReceiver is receiving an Intent
     * broadcast.  During this time you can use the other methods on
     * BroadcastReceiver to view/modify the current result values.  This method
     * is always called within the main thread of its process, unless you   默认是在主线程中回调
     * explicitly asked for it to be scheduled on a different thread using
     * {@link android.content.Context#registerReceiver(BroadcastReceiver,
     * IntentFilter, String, android.os.Handler)}. When it runs on the main
     * thread you should
     * never perform long-running operations in it (there is a timeout of
     * 10 seconds that the system allows before considering the receiver to
     * be blocked and a candidate to be killed). You cannot launch a popup dialog
     * in your implementation of onReceive().
     *
     * <p><b>If this BroadcastReceiver was launched through a &lt;receiver&gt; tag,    async相关
     * then the object is no longer alive after returning from this
     * function.</b> This means you should not perform any operations that
     * return a result to you asynchronously. If you need to perform any follow up
     * background work, schedule a {@link android.app.job.JobService} with
     * {@link android.app.job.JobScheduler}.
     *
     * If you wish to interact with a service that is already running and previously
     * bound using {@link android.content.Context#bindService(Intent, ServiceConnection, int) bindService()},
     * you can use {@link #peekService}.
     *
     * <p>The Intent filters used in {@link android.content.Context#registerReceiver}
     * and in application manifests are <em>not</em> guaranteed to be exclusive. They
     * are hints to the operating system about how to find suitable recipients. It is
     * possible for senders to force delivery to specific recipients, bypassing filter
     * resolution.  For this reason, {@link #onReceive(Context, Intent) onReceive()}
     * implementations should respond only to known actions, ignoring any unexpected
     * Intents that they may receive.
     *
     * @param context The Context in which the receiver is running.
     * @param intent The Intent being received.
     */
    public abstract void onReceive(Context context, Intent intent);

    /**
     * This can be called by an application in {@link #onReceive} to allow
     * it to keep the broadcast active after returning from that function.
     * This does <em>not</em> change the expectation of being relatively
     * responsive to the broadcast, but does allow
     * the implementation to move work related to it over to another thread
     * to avoid glitching the main UI thread due to disk IO.
     *
     * <p>As a general rule, broadcast receivers are allowed to run for up to 10 seconds
     * before they system will consider them non-responsive and ANR the app.  Since these usually
     * execute on the app's main thread, they are already bound by the ~5 second time limit
     * of various operations that can happen there (not to mention just avoiding UI jank), so
     * the receive limit is generally not of concern.  However, once you use {@code goAsync}, though
     * able to be off the main thread, the broadcast execution limit still applies, and that
     * includes the time spent between calling this method and ultimately
     * {@link PendingResult#finish() PendingResult.finish()}.</p>
     *
     * <p>If you are taking advantage of this method to have more time to execute, it is useful
     * to know that the available time can be longer in certain situations.  In particular, if
     * the broadcast you are receiving is not a foreground broadcast (that is, the sender has not
     * used {@link Intent#FLAG_RECEIVER_FOREGROUND}), then more time is allowed for the receivers
     * to run, allowing them to execute for 30 seconds or even a bit more.  This is something that
     * receivers should rarely take advantage of (long work should be punted to another system
     * facility such as {@link android.app.job.JobScheduler}, {@link android.app.Service}, or
     * see especially {@link android.support.v4.app.JobIntentService}), but can be useful in
     * certain rare cases where it is necessary to do some work as soon as the broadcast is
     * delivered.  Keep in mind that the work you do here will block further broadcasts until
     * it completes, so taking advantage of this at all excessively can be counter-productive
     * and cause later events to be received more slowly.</p>
     *
     * @return Returns a {@link PendingResult} representing the result of
     * the active broadcast.  The BroadcastRecord itself is no longer active;
     * all data and other interaction must go through {@link PendingResult}
     * APIs.  The {@link PendingResult#finish PendingResult.finish()} method
     * must be called once processing of the broadcast is done.
     */
    public final PendingResult goAsync() {
        PendingResult res = mPendingResult;
        mPendingResult = null;
        return res;
    }

}