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

#ifndef ZT_JNICACHE_H_
#define ZT_JNICACHE_H_

#include <jni.h>
#include <map>
#include <string>



class JniCache {
public:
    JniCache();
    JniCache(JavaVM *jvm);
    ~JniCache();

    void setJavaVM(JavaVM *jvm);
    void clearCache();

    jclass findClass(const std::string &name);
    jmethodID findMethod(jclass cls, const std::string &methodName, const std::string &methodSig);
    jmethodID findStaticMethod(jclass cls, const std::string &methodName, const std::string &methodSig);
    jfieldID findField(jclass cls, const std::string &fieldName, const std::string &typeStr);
    jfieldID findStaticField(jclass cls, const std::string &fieldName, const std::string &typeStr);
private:
    typedef std::map<std::string, jmethodID> MethodMap;
    typedef std::map<std::string, jfieldID> FieldMap;
    typedef std::map<std::string, jclass> ClassMap;

    JavaVM *m_jvm;
    ClassMap m_classes;
    FieldMap m_fields;
    FieldMap m_staticFields;
    MethodMap m_methods;
    MethodMap m_staticMethods;

};

#endif