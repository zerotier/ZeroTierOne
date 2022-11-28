/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include "ZT_jnilookup.h"
#include "ZT_jniutils.h"

JniLookup::JniLookup()
    : m_jvm(NULL)
{
    LOGV("JNI Cache Created");
}

JniLookup::JniLookup(JavaVM *jvm)
    : m_jvm(jvm)
{
    LOGV("JNI Cache Created");
}

JniLookup::~JniLookup()
{
    LOGV("JNI Cache Destroyed");
}


void JniLookup::setJavaVM(JavaVM *jvm)
{ 
    LOGV("Assigned JVM to object");
    m_jvm = jvm; 
}


jclass JniLookup::findClass(const std::string &name)
{
    if(!m_jvm)
        return NULL;

    // get the class from the JVM
    JNIEnv *env = NULL;
    if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        LOGE("Error retrieving JNI Environment");
        return NULL;
    }
    const char *c = name.c_str();
    jclass cls = env->FindClass(c);
    if(env->ExceptionCheck())
    {
        LOGE("Error finding class: %s", name.c_str());
        return NULL;
    }

    return cls;
}


jmethodID JniLookup::findMethod(jclass cls, const std::string &methodName, const std::string &methodSig)
{
    if(!m_jvm)
        return NULL;

    JNIEnv *env = NULL;
    if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        return NULL;
    }

    jmethodID mid = env->GetMethodID(cls, methodName.c_str(), methodSig.c_str());
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    return mid;
}

jmethodID JniLookup::findStaticMethod(jclass cls, const std::string &methodName, const std::string &methodSig)
{
    if(!m_jvm)
        return NULL;

    JNIEnv *env = NULL;
    if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        return NULL;
    }

    jmethodID mid = env->GetStaticMethodID(cls, methodName.c_str(), methodSig.c_str());
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    return mid;
}

jfieldID JniLookup::findField(jclass cls, const std::string &fieldName, const std::string &typeStr)
{
    if(!m_jvm)
        return NULL;

    JNIEnv *env = NULL;
    if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        return NULL;
    }

    jfieldID fid = env->GetFieldID(cls, fieldName.c_str(), typeStr.c_str());
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    return fid;
}

jfieldID JniLookup::findStaticField(jclass cls, const std::string &fieldName, const std::string &typeStr)
{
    if(!m_jvm)
        return NULL;

    JNIEnv *env = NULL;
    if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        return NULL;
    }

    jfieldID fid = env->GetStaticFieldID(cls, fieldName.c_str(), typeStr.c_str());
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    return fid;
}