//
// Created by Grant Limberg on 10/21/20.
//

#include "ZT_jniarray.h"
#include <vector>
#include <string>
#include <cassert>

jclass java_util_ArrayList;
jmethodID java_util_ArrayList_;
jmethodID java_util_ArrayList_size;
jmethodID java_util_ArrayList_get;
jmethodID java_util_ArrayList_add;

void InitListJNI(JNIEnv* env) {
    java_util_ArrayList      = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
    java_util_ArrayList_     = env->GetMethodID(java_util_ArrayList, "<init>", "(I)V");
    java_util_ArrayList_size = env->GetMethodID (java_util_ArrayList, "size", "()I");
    java_util_ArrayList_get  = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");
    java_util_ArrayList_add  = env->GetMethodID(java_util_ArrayList, "add", "(Ljava/lang/Object;)Z");
}

jclass ListJNI::getListClass(JNIEnv* env) {
    jclass jclazz = env->FindClass("java/util/List");
    assert(jclazz != nullptr);
    return jclazz;
}

jclass ListJNI::getArrayListClass(JNIEnv* env) {
    jclass jclazz = env->FindClass("java/util/ArrayList");
    assert(jclazz != nullptr);
    return jclazz;
}

jclass ListJNI::getIteratorClass(JNIEnv* env) {
    jclass jclazz = env->FindClass("java/util/Iterator");
    assert(jclazz != nullptr);
    return jclazz;
}

jmethodID ListJNI::getIteratorMethod(JNIEnv* env) {
    static jmethodID mid = env->GetMethodID(
            getListClass(env), "iterator", "()Ljava/util/Iterator;");
    assert(mid != nullptr);
    return mid;
}

jmethodID ListJNI::getHasNextMethod(JNIEnv* env) {
    static jmethodID mid = env->GetMethodID(
            getIteratorClass(env), "hasNext", "()Z");
    assert(mid != nullptr);
    return mid;
}

jmethodID ListJNI::getNextMethod(JNIEnv* env) {
    static jmethodID mid = env->GetMethodID(
            getIteratorClass(env), "next", "()Ljava/lang/Object;");
    assert(mid != nullptr);
    return mid;
}

jmethodID ListJNI::getArrayListConstructorMethodId(JNIEnv* env, jclass jclazz) {
    static jmethodID mid = env->GetMethodID(
            jclazz, "<init>", "(I)V");
    assert(mid != nullptr);
    return mid;
}

jmethodID ListJNI::getListAddMethodId(JNIEnv* env) {
    static jmethodID mid = env->GetMethodID(
            getListClass(env), "add", "(Ljava/lang/Object;)Z");
    assert(mid != nullptr);
    return mid;
}

jclass ByteJNI::getByteClass(JNIEnv* env) {
    jclass jclazz = env->FindClass("java/lang/Byte");
    assert(jclazz != nullptr);
    return jclazz;
}

jmethodID ByteJNI::getByteValueMethod(JNIEnv* env) {
    static jmethodID mid = env->GetMethodID(
            getByteClass(env), "byteValue", "()B");
    assert(mid != nullptr);
    return mid;
}

jobject cppToJava(JNIEnv* env, std::vector<std::string> vector) {
    jobject result = env->NewObject(java_util_ArrayList, java_util_ArrayList_, vector.size());
    for (std::string s: vector) {
        jstring element = env->NewStringUTF(s.c_str());
        env->CallBooleanMethod(result, java_util_ArrayList_add, element);
        env->DeleteLocalRef(element);
    }
    return result;
}

std::vector<std::string> javaToCpp(JNIEnv* env, jobject arrayList) {
    jint len = env->CallIntMethod(arrayList, java_util_ArrayList_size);
    std::vector<std::string> result;
    result.reserve(len);
    for (jint i=0; i<len; i++) {
        jstring element = static_cast<jstring>(env->CallObjectMethod(arrayList, java_util_ArrayList_get, i));
        const char* pchars = env->GetStringUTFChars(element, nullptr);
        result.emplace_back(pchars);
        env->ReleaseStringUTFChars(element, pchars);
        env->DeleteLocalRef(element);
    }
    return result;
}
