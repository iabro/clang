// RUN: %clang_analyze_cc1 -fblocks -analyze -analyzer-output=text\
// RUN:                    -analyzer-checker=core,osx -verify %s

struct OSMetaClass;

#define OS_CONSUME __attribute__((os_consumed))
#define OS_RETURNS_RETAINED __attribute__((os_returns_retained))
#define OS_RETURNS_NOT_RETAINED __attribute__((os_returns_not_retained))
#define OS_CONSUMES_THIS __attribute__((os_consumes_this))

#define OSTypeID(type)   (type::metaClass)

#define OSDynamicCast(type, inst)   \
    ((type *) OSMetaClassBase::safeMetaCast((inst), OSTypeID(type)))

using size_t = decltype(sizeof(int));

struct OSObject {
  virtual void retain();
  virtual void release() {};
  virtual void free();
  virtual ~OSObject(){}

  unsigned int foo() { return 42; }

  virtual OS_RETURNS_NOT_RETAINED OSObject *identity();

  static OSObject *generateObject(int);

  static OSObject *getObject();
  static OSObject *GetObject();

  static void * operator new(size_t size);

  static const OSMetaClass * const metaClass;
};

struct OSIterator : public OSObject {

  static const OSMetaClass * const metaClass;
};

struct OSArray : public OSObject {
  unsigned int getCount();

  OSIterator * getIterator();

  OSObject *identity() override;

  virtual OSObject *generateObject(OSObject *input);

  virtual void consumeReference(OS_CONSUME OSArray *other);

  void putIntoArray(OSArray *array) OS_CONSUMES_THIS;

  template <typename T>
  void putIntoT(T *owner) OS_CONSUMES_THIS;

  static OSArray *generateArrayHasCode() {
    return new OSArray;
  }

  static OSArray *withCapacity(unsigned int capacity);
  static void consumeArray(OS_CONSUME OSArray * array);

  static OSArray* consumeArrayHasCode(OS_CONSUME OSArray * array) {
    return nullptr;
  }

  static OS_RETURNS_NOT_RETAINED OSArray *MaskedGetter();
  static OS_RETURNS_RETAINED OSArray *getOoopsActuallyCreate();

  static const OSMetaClass * const metaClass;
};

struct MyArray : public OSArray {
  void consumeReference(OSArray *other) override;

  OSObject *identity() override;

  OSObject *generateObject(OSObject *input) override;
};

struct OtherStruct {
  static void doNothingToArray(OSArray *array);
  OtherStruct(OSArray *arr);
};

struct OSMetaClassBase {
  static OSObject *safeMetaCast(const OSObject *inst, const OSMetaClass *meta);
};

void escape(void *);
void escape_with_source(void *p) {}
bool coin();

bool os_consume_violation_two_args(OS_CONSUME OSObject *obj, bool extra) {
  if (coin()) { // expected-note{{Assuming the condition is false}}
                // expected-note@-1{{Taking false branch}}
    escape(obj);
    return true;
  }
  return false; // expected-note{{Parameter 'obj' is marked as consuming, but the function did not consume the reference}}
}

bool os_consume_violation(OS_CONSUME OSObject *obj) {
  if (coin()) { // expected-note{{Assuming the condition is false}}
                // expected-note@-1{{Taking false branch}}
    escape(obj);
    return true;
  }
  return false; // expected-note{{Parameter 'obj' is marked as consuming, but the function did not consume the reference}}
}

void os_consume_ok(OS_CONSUME OSObject *obj) {
  escape(obj);
}

void use_os_consume_violation() {
  OSObject *obj = new OSObject; // expected-note{{Operator 'new' returns an OSObject of type OSObject with a +1 retain count}}
  os_consume_violation(obj); // expected-note{{Calling 'os_consume_violation'}}
                             // expected-note@-1{{Returning from 'os_consume_violation'}}
} // expected-note{{Object leaked: object allocated and stored into 'obj' is not referenced later in this execution path and has a retain count of +1}}
  // expected-warning@-1{{Potential leak of an object stored into 'obj'}}

void use_os_consume_violation_two_args() {
  OSObject *obj = new OSObject; // expected-note{{Operator 'new' returns an OSObject of type OSObject with a +1 retain count}}
  os_consume_violation_two_args(obj, coin()); // expected-note{{Calling 'os_consume_violation_two_args'}}
                             // expected-note@-1{{Returning from 'os_consume_violation_two_args'}}
} // expected-note{{Object leaked: object allocated and stored into 'obj' is not referenced later in this execution path and has a retain count of +1}}
  // expected-warning@-1{{Potential leak of an object stored into 'obj'}}

void use_os_consume_ok() {
  OSObject *obj = new OSObject;
  os_consume_ok(obj);
}

void test_escaping_into_voidstar() {
  OSObject *obj = new OSObject;
  escape(obj);
}

void test_escape_has_source() {
  OSObject *obj = new OSObject;
  if (obj)
    escape_with_source(obj);
  return;
}

void test_no_infinite_check_recursion(MyArray *arr) {
  OSObject *input = new OSObject;
  OSObject *o = arr->generateObject(input);
  o->release();
  input->release();
}


void check_param_attribute_propagation(MyArray *parent) {
  OSArray *arr = new OSArray;
  parent->consumeReference(arr);
}

unsigned int check_attribute_propagation(OSArray *arr) {
  OSObject *other = arr->identity();
  OSArray *casted = OSDynamicCast(OSArray, other);
  if (casted)
    return casted->getCount();
  return 0;
}

unsigned int check_attribute_indirect_propagation(MyArray *arr) {
  OSObject *other = arr->identity();
  OSArray *casted = OSDynamicCast(OSArray, other);
  if (casted)
    return casted->getCount();
  return 0;
}

void check_consumes_this(OSArray *owner) {
  OSArray *arr = new OSArray;
  arr->putIntoArray(owner);
}

void check_consumes_this_with_template(OSArray *owner) {
  OSArray *arr = new OSArray;
  arr->putIntoT(owner);
}

void check_free_no_error() {
  OSArray *arr = OSArray::withCapacity(10);
  arr->retain();
  arr->retain();
  arr->retain();
  arr->free();
}

void check_free_use_after_free() {
  OSArray *arr = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
  arr->retain(); // expected-note{{Reference count incremented. The object now has a +2 retain count}}
  arr->free(); // expected-note{{Object released}}
  arr->retain(); // expected-warning{{Reference-counted object is used after it is released}}
                 // expected-note@-1{{Reference-counted object is used after it is released}}
}

unsigned int check_leak_explicit_new() {
  OSArray *arr = new OSArray; // expected-note{{Operator 'new' returns an OSObject of type OSArray with a +1 retain count}}
  return arr->getCount(); // expected-note{{Object leaked: object allocated and stored into 'arr' is not referenced later in this execution path and has a retain count of +1}}
                          // expected-warning@-1{{Potential leak of an object stored into 'arr'}}
}

unsigned int check_leak_factory() {
  OSArray *arr = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
  return arr->getCount(); // expected-note{{Object leaked: object allocated and stored into 'arr' is not referenced later in this execution path and has a retain count of +1}}
                          // expected-warning@-1{{Potential leak of an object stored into 'arr'}}
}

void check_get_object() {
  OSObject::getObject();
}

void check_Get_object() {
  OSObject::GetObject();
}

void check_custom_iterator_rule(OSArray *arr) {
  OSIterator *it = arr->getIterator();
  it->release();
}

void check_iterator_leak(OSArray *arr) {
  arr->getIterator(); // expected-note{{Call to method 'OSArray::getIterator' returns an OSObject of type OSIterator with a +1 retain count}}
} // expected-note{{Object leaked: allocated object of type OSIterator is not referenced later}}
  // expected-warning@-1{{Potential leak of an object of type OSIterator}}

void check_no_invalidation() {
  OSArray *arr = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
  OtherStruct::doNothingToArray(arr);
} // expected-warning{{Potential leak of an object stored into 'arr'}}
  // expected-note@-1{{Object leaked}}

void check_no_invalidation_other_struct() {
  OSArray *arr = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
  OtherStruct other(arr); // expected-warning{{Potential leak}}
                          // expected-note@-1{{Object leaked}}
}

struct ArrayOwner : public OSObject {
  OSArray *arr;
  ArrayOwner(OSArray *arr) : arr(arr) {}

  static ArrayOwner* create(OSArray *arr) {
    return new ArrayOwner(arr);
  }

  OSArray *getArray() {
    return arr;
  }

  OSArray *createArray() {
    return OSArray::withCapacity(10);
  }

  OSArray *createArraySourceUnknown();

  OSArray *getArraySourceUnknown();
};

OSArray *generateArray() {
  return OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
                                    // expected-note@-1{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
}

unsigned int check_leak_good_error_message() {
  unsigned int out;
  {
    OSArray *leaked = generateArray(); // expected-note{{Calling 'generateArray'}}
                                       // expected-note@-1{{Returning from 'generateArray'}}
    out = leaked->getCount(); // expected-warning{{Potential leak of an object stored into 'leaked'}}
                              // expected-note@-1{{Object leaked: object allocated and stored into 'leaked' is not referenced later in this execution path and has a retain count of +1}}
  }
  return out;
}

unsigned int check_leak_msg_temporary() {
  return generateArray()->getCount(); // expected-warning{{Potential leak of an object}}
                                      // expected-note@-1{{Calling 'generateArray'}}
                                      // expected-note@-2{{Returning from 'generateArray'}}
                                      // expected-note@-3{{Object leaked: allocated object of type OSArray is not referenced later in this execution path and has a retain count of +1}}
}

void check_confusing_getters() {
  OSArray *arr = OSArray::withCapacity(10);

  ArrayOwner *AO = ArrayOwner::create(arr);
  AO->getArray();

  AO->release();
  arr->release();
}

void check_rc_consumed() {
  OSArray *arr = OSArray::withCapacity(10);
  OSArray::consumeArray(arr);
}

void check_rc_consume_temporary() {
  OSArray::consumeArray(OSArray::withCapacity(10));
}

void check_rc_getter() {
  OSArray *arr = OSArray::MaskedGetter();
  (void)arr;
}

void check_rc_create() {
  OSArray *arr = OSArray::getOoopsActuallyCreate();
  arr->release();
}


void check_dynamic_cast() {
  OSArray *arr = OSDynamicCast(OSArray, OSObject::generateObject(1));
  arr->release();
}

unsigned int check_dynamic_cast_no_null_on_orig(OSObject *obj) {
  OSArray *arr = OSDynamicCast(OSArray, obj);
  if (arr) {
    return arr->getCount();
  } else {

    // The fact that dynamic cast has failed should not imply that
    // the input object was null.
    return obj->foo(); // no-warning
  }
}

void check_dynamic_cast_null_branch(OSObject *obj) {
  OSArray *arr1 = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject}}
  OSArray *arr = OSDynamicCast(OSArray, obj);
  if (!arr) // expected-note{{Taking true branch}}
    return; // expected-warning{{Potential leak of an object stored into 'arr1'}}
            // expected-note@-1{{Object leaked}}
  arr1->release();
}

void check_dynamic_cast_null_check() {
  OSArray *arr = OSDynamicCast(OSArray, OSObject::generateObject(1)); // expected-note{{Call to method 'OSObject::generateObject' returns an OSObject}}
    // expected-warning@-1{{Potential leak of an object}}
    // expected-note@-2{{Object leaked}}
  if (!arr)
    return;
  arr->release();
}

void use_after_release() {
  OSArray *arr = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
  arr->release(); // expected-note{{Object released}}
  arr->getCount(); // expected-warning{{Reference-counted object is used after it is released}}
                   // expected-note@-1{{Reference-counted object is used after it is released}}
}

void potential_leak() {
  OSArray *arr = OSArray::withCapacity(10); // expected-note{{Call to method 'OSArray::withCapacity' returns an OSObject of type OSArray with a +1 retain count}}
  arr->retain(); // expected-note{{Reference count incremented. The object now has a +2 retain count}}
  arr->release(); // expected-note{{Reference count decremented. The object now has a +1 retain count}}
  arr->getCount();
} // expected-warning{{Potential leak of an object stored into 'arr'}}
  // expected-note@-1{{Object leaked: object allocated and stored into 'arr' is not referenced later in this execution path and has a retain count of +1}}

void proper_cleanup() {
  OSArray *arr = OSArray::withCapacity(10); // +1
  arr->retain(); // +2
  arr->release(); // +1
  arr->getCount();
  arr->release(); // 0
}

unsigned int no_warning_on_getter(ArrayOwner *owner) {
  OSArray *arr = owner->getArray();
  return arr->getCount();
}

unsigned int warn_on_overrelease(ArrayOwner *owner) {
  // FIXME: summaries are not applied in case the source of the getter/setter
  // is known.
  // rdar://45681203
  OSArray *arr = owner->getArray();
  arr->release();
  return arr->getCount();
}

unsigned int nowarn_on_release_of_created(ArrayOwner *owner) {
  OSArray *arr = owner->createArray();
  unsigned int out = arr->getCount();
  arr->release();
  return out;
}

unsigned int nowarn_on_release_of_created_source_unknown(ArrayOwner *owner) {
  OSArray *arr = owner->createArraySourceUnknown();
  unsigned int out = arr->getCount();
  arr->release();
  return out;
}

unsigned int no_warn_ok_release(ArrayOwner *owner) {
  OSArray *arr = owner->getArray(); // +0
  arr->retain(); // +1
  arr->release(); // +0
  return arr->getCount(); // no-warning
}

unsigned int warn_on_overrelease_with_unknown_source(ArrayOwner *owner) {
  OSArray *arr = owner->getArraySourceUnknown(); // expected-note{{Call to method 'ArrayOwner::getArraySourceUnknown' returns an OSObject of type OSArray with a +0 retain count}}
  arr->release(); // expected-warning{{Incorrect decrement of the reference count of an object that is not owned at this point by the caller}}
                  // expected-note@-1{{Incorrect decrement of the reference count of an object that is not owned at this point by the caller}}
  return arr->getCount();
}

unsigned int ok_release_with_unknown_source(ArrayOwner *owner) {
  OSArray *arr = owner->getArraySourceUnknown(); // +0
  arr->retain(); // +1
  arr->release(); // +0
  return arr->getCount();
}

OSObject *getObject();
typedef bool (^Blk)(OSObject *);

void test_escape_to_unknown_block(Blk blk) {
  blk(getObject()); // no-crash
}

