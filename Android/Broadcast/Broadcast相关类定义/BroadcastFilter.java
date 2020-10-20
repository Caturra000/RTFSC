
// 一个意义不大的类，用于dump
final class BroadcastFilter extends IntentFilter {
    final ReceiverList receiverList;
    final String packageName;
    final String requiredPermission;
    final int owningUid;
    final int owningUserId; // Uid != UserId
    //... 省略

    // 剩余的是dump方法和writeToProto通信
}