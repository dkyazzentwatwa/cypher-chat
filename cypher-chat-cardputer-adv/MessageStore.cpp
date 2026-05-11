#include "MessageStore.h"
#include <cstring>

MessageStore messageStore;

MessageStore::MessageStore()
  : _count(0),
    _next(0) {
  memset(_items, 0, sizeof(_items));
}

void MessageStore::add(const char* text, bool outgoing, bool emergency) {
  if (!text || strlen(text) == 0) {
    return;
  }

  StoredMessage& item = _items[_next];
  strncpy(item.text, text, sizeof(item.text) - 1);
  item.text[sizeof(item.text) - 1] = '\0';
  parseSender(text, item.sender, sizeof(item.sender));
  item.timestamp = millis();
  item.outgoing = outgoing;
  item.emergency = emergency;

  _next = (_next + 1) % FIELD_MESSAGE_STORE_SIZE;
  if (_count < FIELD_MESSAGE_STORE_SIZE) {
    _count++;
  }
}

void MessageStore::clear() {
  memset(_items, 0, sizeof(_items));
  _count = 0;
  _next = 0;
}

int MessageStore::count(MessageFilter filter) const {
  int total = 0;
  for (int i = 0; i < _count; i++) {
    int idx = (_next - 1 - i + FIELD_MESSAGE_STORE_SIZE) % FIELD_MESSAGE_STORE_SIZE;
    if (matches(_items[idx], filter)) {
      total++;
    }
  }
  return total;
}

bool MessageStore::getNewest(int visibleIndex, MessageFilter filter, StoredMessage& out) const {
  if (visibleIndex < 0) {
    return false;
  }

  int seen = 0;
  for (int i = 0; i < _count; i++) {
    int idx = (_next - 1 - i + FIELD_MESSAGE_STORE_SIZE) % FIELD_MESSAGE_STORE_SIZE;
    if (!matches(_items[idx], filter)) {
      continue;
    }
    if (seen == visibleIndex) {
      out = _items[idx];
      return true;
    }
    seen++;
  }
  return false;
}

bool MessageStore::formatNewest(int visibleIndex, MessageFilter filter, char* out, size_t outSize) const {
  if (!out || outSize == 0) {
    return false;
  }

  StoredMessage item;
  if (!getNewest(visibleIndex, filter, item)) {
    return false;
  }

  char age[10];
  formatAge(item.timestamp, age, sizeof(age));
  snprintf(out, outSize, "%s %c%s %s",
           age,
           item.outgoing ? '>' : '<',
           item.emergency ? "!" : " ",
           item.text);
  return true;
}

bool MessageStore::lastOutgoing(char* out, size_t outSize) const {
  if (!out || outSize == 0) {
    return false;
  }

  for (int i = 0; i < _count; i++) {
    int idx = (_next - 1 - i + FIELD_MESSAGE_STORE_SIZE) % FIELD_MESSAGE_STORE_SIZE;
    if (_items[idx].outgoing) {
      strncpy(out, _items[idx].text, outSize - 1);
      out[outSize - 1] = '\0';
      return true;
    }
  }
  return false;
}

const char* MessageStore::filterName(MessageFilter filter) const {
  switch (filter) {
    case MSG_FILTER_INCOMING: return "incoming";
    case MSG_FILTER_OUTGOING: return "outgoing";
    case MSG_FILTER_EMERGENCY: return "emergency";
    case MSG_FILTER_ALL:
    default: return "all";
  }
}

bool MessageStore::matches(const StoredMessage& item, MessageFilter filter) const {
  switch (filter) {
    case MSG_FILTER_INCOMING: return !item.outgoing;
    case MSG_FILTER_OUTGOING: return item.outgoing;
    case MSG_FILTER_EMERGENCY: return item.emergency;
    case MSG_FILTER_ALL:
    default: return true;
  }
}

void MessageStore::parseSender(const char* text, char* out, size_t outSize) const {
  if (!out || outSize == 0) {
    return;
  }

  out[0] = '\0';
  if (!text) {
    return;
  }

  size_t len = 0;
  while (text[len] && text[len] != ':' && len < outSize - 1) {
    out[len] = text[len];
    len++;
  }
  out[len] = '\0';
}

void MessageStore::formatAge(unsigned long timestamp, char* out, size_t outSize) const {
  unsigned long age = (millis() - timestamp) / 1000;
  if (age < 60) {
    snprintf(out, outSize, "%lus", age);
  } else if (age < 3600) {
    snprintf(out, outSize, "%lum", age / 60);
  } else {
    snprintf(out, outSize, "%luh", age / 3600);
  }
}
