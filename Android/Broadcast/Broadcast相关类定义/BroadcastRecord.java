// BroadcastRecord涉及到核心流程，包含自身属性和已经归类好的receivers
final class BroadcastRecord { // extends Binder
// 基本信息

    final Intent intent;
    final ComponentName targetComp;
    final String callerPackage;
    final int callingPid;
    final int callingUid;

// 自身属性

    final boolean ordered;
    final boolean sticky;
    final boolean initialSticky; // 第一个sticky broadcast？       TODO

    boolean deferred; 
    boolean timeoutExempt; // 

    // type permission...

    // result...

// 记录log的时间

    long enqueueClockTime;
    long dispatchTime;
    //......


// 处理流程相关    

    final List receivers; // 包括BroadcastFilter和ResolveInfo     TODO 整理receivers构造流程
    int nextReceiver;
    IBinder receiver;     // currently running                    TODO find usage 
    BroadcastQueue queue; // 处理这个record的队列

    BroadcastFilter curFilter;
    ProcessRecord curApp;
    ComponentName curComponent;
    ActivityInfo curReceiver; // currently running的Reciver的info


// 状态机                                                         TODO 整理状态转移的流程

    static final int IDLE = 0;
    static final int APP_RECEIVE = 1;
    static final int CALL_IN_RECEIVE = 2;
    static final int CALL_DONE_RECEIVE = 3;
    static final int WAITING_SERVICES = 4;

    static final int DELIVER_PENDING = 0;
    static final int DELIVER_DELIVERED = 1;
    static final int DELIVER_SKIPPED = 2;
    static final int DELIVER_TIMEOUT = 3;
}