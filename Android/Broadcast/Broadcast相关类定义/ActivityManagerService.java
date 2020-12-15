
    /**
     * Keeps track of all IIntentReceivers that have been registered for broadcasts.
     * Hash keys are the receiver IBinder, hash value is a ReceiverList.
     */
    // 用于保存IIntentReceiver和对应ReceiverList的关系
    final HashMap<IBinder, ReceiverList> mRegisteredReceivers = new HashMap<>();
