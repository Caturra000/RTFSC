
// 一个意义不大的类，用于dump
// 只是简单解决AMS中receiver与filter是一对多关系，但是filter无法找到属于哪个receiver才额外设计的类
final class BroadcastFilter extends IntentFilter {
    final ReceiverList receiverList; // 归属的receiverList中的receiver
    final String packageName;
    final String requiredPermission;
    final int owningUid;
    final int owningUserId; // Uid != UserId
    //... 省略

    // 剩余的是dump方法和writeToProto通信
}