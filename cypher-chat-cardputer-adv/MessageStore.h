#pragma once

#include "Config_Cardputer.h"

enum MessageFilter : uint8_t {
  MSG_FILTER_ALL,
  MSG_FILTER_INCOMING,
  MSG_FILTER_OUTGOING,
  MSG_FILTER_EMERGENCY
};

struct StoredMessage {
  char text[MAX_MESSAGE_SIZE];
  char sender[MAX_UNIT_NAME_LEN + 1];
  unsigned long timestamp;
  bool outgoing;
  bool emergency;
};

class MessageStore {
public:
  MessageStore();

  void add(const char* text, bool outgoing, bool emergency);
  void clear();
  int count(MessageFilter filter = MSG_FILTER_ALL) const;
  bool getNewest(int visibleIndex, MessageFilter filter, StoredMessage& out) const;
  bool formatNewest(int visibleIndex, MessageFilter filter, char* out, size_t outSize) const;
  bool lastOutgoing(char* out, size_t outSize) const;
  const char* filterName(MessageFilter filter) const;

private:
  StoredMessage _items[FIELD_MESSAGE_STORE_SIZE];
  int _count;
  int _next;

  bool matches(const StoredMessage& item, MessageFilter filter) const;
  void parseSender(const char* text, char* out, size_t outSize) const;
  void formatAge(unsigned long timestamp, char* out, size_t outSize) const;
};

extern MessageStore messageStore;
