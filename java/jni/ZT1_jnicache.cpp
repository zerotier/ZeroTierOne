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

#include "ZT1_jnicache.h"
#include "ZT1_jniutils.h"

JniCache::JniCache()
    : m_jvm(NULL)
    , m_classes()
    , m_fields()
    , m_staticFields()
    , m_methods()
    , m_staticMethods()
{
    LOGV("JNI Cache Created");
}

JniCache::JniCache(JavaVM *jvm)
    : m_jvm(jvm)
    , m_classes()
    , m_fields()
    , m_staticFields()
    , m_methods()
    , m_staticMethods()
{
    LOGV("JNI Cache Created");
}

JniCache::~JniCache()
{
    LOGV("JNI Cache Destroyed");
    clearCache();
}

void JniCache::clearCache()
{
    if(m_jvm)
    {
        JNIEnv *env = NULL;
        if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
            return;

        for(ClassMap::iterator iter = m_classes.begin(), end = m_classes.end();
            iter != end; ++iter)
        {
            env->DeleteGlobalRef(iter->second);
        }
    }

    m_classes.clear();
    m_fields.clear();
    m_staticFields.clear();
    m_methods.clear();
    m_staticMethods.clear();
}

void JniCache::setJavaVM(JavaVM *jvm)
{ 
    LOGV("Assigned JVM to object");
    m_jvm = jvm; 
}


jclass JniCache::findClass(const std::string &name)
{
    if(!m_jvm)
        return NULL;

    ClassMap::iterator found = m_classes.find(name);

    if(found == m_classes.end())
    {
        // get the class from the JVM
        JNIEnv *env = NULL;
        if(m_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
        {
            LOGE("Error retreiving JNI Environment");
            return NULL;
        }

        jclass localCls = env->FindClass(name.c_str());
        if(env->ExceptionCheck())
        {
            LOGE("Error finding class: %s", name.c_str());
            return NULL;
        }

        jclass cls = (jclass)env->NewGlobalRef(localCls);

        m_classes.insert(std::make_pair(name, cls));

        return cls;
    }

    LOGV("Returning cached %s", name.c_str());
    return found->second;
}


jmethodID JniCache::findMethod(jclass cls, const std::string &methodName, const std::string &methodSig)
{
    if(!m_jvm)
        return NULL;

    std::string id = methodName + methodSig;

    MethodMap::iterator found = m_methods.find(id);
    if(found == m_methods.end())
    {
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

        m_methods.insert(std::make_pair(id, mid));

        return mid;
    }

    return found->second;
}

jmethodID JniCache::findStaticMethod(jclass cls, const std::string &methodName, const std::string &methodSig)
{
    if(!m_jvm)
        return NULL;

    std::string id = methodName + methodSig;

    MethodMap::iterator found = m_staticMethods.find(id);
    if(found == m_staticMethods.end())
    {
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

        m_staticMethods.insert(std::make_pair(id, mid));

        return mid;
    }

    return found->second;
}

jfieldID JniCache::findField(jclass cls, const std::string &fieldName, const std::string &typeStr)
{
    if(!m_jvm)
        return NULL;

    std::string id = fieldName + typeStr;

    FieldMap::iterator found = m_fields.find(id);
    if(found == m_fields.end())
    {
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

        m_fields.insert(std::make_pair(id, fid));

        return fid;
    }

    return found->second;
}

jfieldID JniCache::findStaticField(jclass cls, const std::string &fieldName, const std::string &typeStr)
{
    if(!m_jvm)
        return NULL;

    std::string id = fieldName + typeStr;

    FieldMap::iterator found = m_staticFields.find(id);
    if(found == m_staticFields.end())
    {
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

        m_staticFields.insert(std::make_pair(id, fid));

        return fid;
    }

    return found->second;
}