// 文件：system/logging/liblog/logger_write.cpp

int __android_log_buf_write(int bufID, int prio, const char* tag, const char* msg) {
  ErrnoRestorer errno_restorer;

  if (!__android_log_is_loggable(prio, tag, ANDROID_LOG_VERBOSE)) {
    return -EPERM;
  }

  __android_log_message log_message = {
      sizeof(__android_log_message), bufID, prio, tag, nullptr, 0, msg};
  __android_log_write_log_message(&log_message);
  return 1;
}

void __android_log_write_log_message(__android_log_message* log_message) {
  ErrnoRestorer errno_restorer;

  if (log_message->buffer_id != LOG_ID_DEFAULT && log_message->buffer_id != LOG_ID_MAIN &&
      log_message->buffer_id != LOG_ID_SYSTEM && log_message->buffer_id != LOG_ID_RADIO &&
      log_message->buffer_id != LOG_ID_CRASH) {
    return;
  }

  if (log_message->tag == nullptr) {
    log_message->tag = GetDefaultTag().c_str();
  }

#if __BIONIC__
  if (log_message->priority == ANDROID_LOG_FATAL) {
    android_set_abort_message(log_message->message);
  }
#endif

  logger_function(log_message);
}


#ifdef __ANDROID__
static __android_logger_function logger_function = __android_log_logd_logger;
#else
static __android_logger_function logger_function = __android_log_stderr_logger;
#endif



void __android_log_logd_logger(const struct __android_log_message* log_message) {
  int buffer_id = log_message->buffer_id == LOG_ID_DEFAULT ? LOG_ID_MAIN : log_message->buffer_id;

  struct iovec vec[3];
  vec[0].iov_base =
      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(&log_message->priority));
  vec[0].iov_len = 1;
  vec[1].iov_base = const_cast<void*>(static_cast<const void*>(log_message->tag));
  vec[1].iov_len = strlen(log_message->tag) + 1;
  vec[2].iov_base = const_cast<void*>(static_cast<const void*>(log_message->message));
  vec[2].iov_len = strlen(log_message->message) + 1;

  write_to_log(static_cast<log_id_t>(buffer_id), vec, 3);
}


#ifdef __ANDROID__
static int write_to_log(log_id_t log_id, struct iovec* vec, size_t nr) {
  int ret;
  struct timespec ts;

  if (log_id == LOG_ID_KERNEL) {
    return -EINVAL;
  }

  clock_gettime(CLOCK_REALTIME, &ts);

  if (log_id == LOG_ID_SECURITY) {
    if (vec[0].iov_len < 4) {
      return -EINVAL;
    }

    ret = check_log_uid_permissions();
    if (ret < 0) {
      return ret;
    }
    if (!__android_log_security()) {
      /* If only we could reset downstream logd counter */
      return -EPERM;
    }
  } else if (log_id == LOG_ID_EVENTS || log_id == LOG_ID_STATS) {
    if (vec[0].iov_len < 4) {
      return -EINVAL;
    }
  }

  ret = LogdWrite(log_id, &ts, vec, nr);
  PmsgWrite(log_id, &ts, vec, nr);

  return ret;
}
#else
static int write_to_log(log_id_t, struct iovec*, size_t) {
  // Non-Android text logs should go to __android_log_stderr_logger, not here.
  // Non-Android binary logs are always dropped.
  return 1;
}
#endif





