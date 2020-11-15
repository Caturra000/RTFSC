// 文件分布：frameworks/base/services/core/java/com/android/server/wm/WindowProcessController.java

// Draft
// 从注释上看是一个用于AM与WM交互的类

/**
 * The Activity Manager (AM) package manages the lifecycle of processes in the system through
 * ProcessRecord. However, it is important for the Window Manager (WM) package to be aware
 * of the processes and their state since it affects how WM manages windows and activities. This
 * class that allows the ProcessRecord object in the AM package to communicate important
 * changes to its state to the WM package in a structured way. WM package also uses
 * {@link WindowProcessListener} to request changes to the process state on the AM side.
 * Note that public calls into this class are assumed to be originating from outside the
 * window manager so the window manager lock is held and appropriate permissions are checked before
 * calls are allowed to proceed.
 */
public class WindowProcessController extends ConfigurationContainer<ConfigurationContainer>
        implements ConfigurationContainerListener {
    private static final String TAG = TAG_WITH_CLASS_NAME ? "WindowProcessController" : TAG_ATM;
    private static final String TAG_RELEASE = TAG + POSTFIX_RELEASE;
    private static final String TAG_CONFIGURATION = TAG + POSTFIX_CONFIGURATION;

    // all about the first app in the process
    final ApplicationInfo mInfo;
    final String mName;
    final int mUid;
    // The process of this application; 0 if none
    private volatile int mPid;
    // user of process.
    final int mUserId;
    // The owner of this window process controller object. Mainly for identification when we
    // communicate back to the activity manager side.
    public final Object mOwner;
    // List of packages running in the process
    final ArraySet<String> mPkgList = new ArraySet<>();
    private final WindowProcessListener mListener;
    private final ActivityTaskManagerService mAtm;
    // The actual proc...  may be null only if 'persistent' is true (in which case we are in the
    // process of launching the app)
    private IApplicationThread mThread;
    // Currently desired scheduling class
    private volatile int mCurSchedGroup;
    // Currently computed process state
    private volatile int mCurProcState = PROCESS_STATE_NONEXISTENT;
    // Last reported process state;
    private volatile int mRepProcState = PROCESS_STATE_NONEXISTENT;
    // are we in the process of crashing?
    private volatile boolean mCrashing;
    // does the app have a not responding dialog?
    private volatile boolean mNotResponding;
    // always keep this application running?
    private volatile boolean mPersistent;
    // The ABI this process was launched with
    private volatile String mRequiredAbi;
    // Running any services that are foreground?
    private volatile boolean mHasForegroundServices;
    // Running any activities that are foreground?
    private volatile boolean mHasForegroundActivities;
    // Are there any client services with activities?
    private volatile boolean mHasClientActivities;
    // Is this process currently showing a non-activity UI that the user is interacting with?
    // E.g. The status bar when it is expanded, but not when it is minimized. When true the process
    // will be set to use the ProcessList#SCHED_GROUP_TOP_APP scheduling group to boost performance.
    private volatile boolean mHasTopUi;
    // Is the process currently showing a non-activity UI that overlays on-top of activity UIs on
    // screen. E.g. display a window of type
    // android.view.WindowManager.LayoutParams#TYPE_APPLICATION_OVERLAY When true the process will
    // oom adj score will be set to ProcessList#PERCEPTIBLE_APP_ADJ at minimum to reduce the chance
    // of the process getting killed.
    private volatile boolean mHasOverlayUi;
    // Want to clean up resources from showing UI?
    private volatile boolean mPendingUiClean;
    // The time we sent the last interaction event
    private volatile long mInteractionEventTime;
    // When we became foreground for interaction purposes
    private volatile long mFgInteractionTime;
    // When (uptime) the process last became unimportant
    private volatile long mWhenUnimportant;
    // was app launched for debugging?
    private volatile boolean mDebugging;
    // Active instrumentation running in process?
    private volatile boolean mInstrumenting;
    // Active instrumentation with background activity starts privilege running in process?
    private volatile boolean mInstrumentingWithBackgroundActivityStartPrivileges;
    // This process it perceptible by the user.
    private volatile boolean mPerceptible;
    // Set to true when process was launched with a wrapper attached
    private volatile boolean mUsingWrapper;
    // Set to true if this process is currently temporarily whitelisted to start activities even if
    // it's not in the foreground
    private volatile boolean mAllowBackgroundActivityStarts;
    // Set of UIDs of clients currently bound to this process
    private volatile ArraySet<Integer> mBoundClientUids = new ArraySet<Integer>();

    // Thread currently set for VR scheduling
    int mVrThreadTid;

    // Whether this process has ever started a service with the BIND_INPUT_METHOD permission.
    private volatile boolean mHasImeService;

    // all activities running in the process
    private final ArrayList<ActivityRecord> mActivities = new ArrayList<>();
    // any tasks this process had run root activities in
    private final ArrayList<Task> mRecentTasks = new ArrayList<>();
    // The most recent top-most activity that was resumed in the process for pre-Q app.
    private ActivityRecord mPreQTopResumedActivity = null;
    // The last time an activity was launched in the process
    private long mLastActivityLaunchTime;
    // The last time an activity was finished in the process while the process participated
    // in a visible task
    private long mLastActivityFinishTime;

    // Last configuration that was reported to the process.
    private final Configuration mLastReportedConfiguration = new Configuration();
    // Configuration that is waiting to be dispatched to the process.
    private Configuration mPendingConfiguration;
    private final Configuration mNewOverrideConfig = new Configuration();
    // Registered display id as a listener to override config change
    private int mDisplayId;
    private ActivityRecord mConfigActivityRecord;
    // Whether the activity config override is allowed for this process.
    private volatile boolean mIsActivityConfigOverrideAllowed = true;
    /**
     * Activities that hosts some UI drawn by the current process. The activities live
     * in another process. This is used to check if the process is currently showing anything
     * visible to the user.
     */
    @Nullable
    private final ArrayList<ActivityRecord> mHostActivities = new ArrayList<>();

    /** Whether our process is currently running a {@link RecentsAnimation} */
    private boolean mRunningRecentsAnimation;

    /** Whether our process is currently running a {@link IRemoteAnimationRunner} */
    private boolean mRunningRemoteAnimation;

    // ...

}