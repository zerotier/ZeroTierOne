//
// Created by Grant Limberg on 10/21/20.
//

#ifndef ZEROTIERANDROID_ZT_JNIARRAY_H
#define ZEROTIERANDROID_ZT_JNIARRAY_H

#include <jni.h>
#include <vector>
#include <string>

extern jclass java_util_ArrayList;
extern jmethodID java_util_ArrayList_;
extern jmethodID java_util_ArrayList_size;
extern jmethodID java_util_ArrayList_get;
extern jmethodID java_util_ArrayList_add;

void InitListJNI(JNIEnv* env);

class ListJNI {
public:
    // Get the java class id of java.util.List.
    static jclass getListClass(JNIEnv* env);

    // Get the java class id of java.util.ArrayList.
    static jclass getArrayListClass(JNIEnv* env);

    // Get the java class id of java.util.Iterator.
    static jclass getIteratorClass(JNIEnv* env);

    // Get the java method id of java.util.List.iterator().
    static jmethodID getIteratorMethod(JNIEnv* env);

    // Get the java method id of java.util.Iterator.hasNext().
    static jmethodID getHasNextMethod(JNIEnv* env);

    // Get the java method id of java.util.Iterator.next().
    static jmethodID getNextMethod(JNIEnv* env);

    // Get the java method id of arrayList constructor.
    static jmethodID getArrayListConstructorMethodId(JNIEnv* env, jclass jclazz);

    // Get the java method id of java.util.List.add().
    static jmethodID getListAddMethodId(JNIEnv* env);
};

class ByteJNI {
public:
    // Get the java class id of java.lang.Byte.
    static jclass getByteClass(JNIEnv* env);

    // Get the java method id of java.lang.Byte.byteValue.
    static jmethodID getByteValueMethod(JNIEnv* env);
};

jobject cppToJava(JNIEnv* env, std::vector<std::string> vector);

std::vector<std::string> javaToCpp(JNIEnv* env, jobject arrayList);

#endif //ZEROTIERANDROID_ZT_JNIARRAY_H
