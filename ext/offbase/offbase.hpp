/*
 * Offbase: a super-minimal in-filesystem JSON object persistence store
 */

#ifndef OFFBASE_HPP__
#define OFFBASE_HPP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include <vector>
#include <string>
#include <map>

#include "json/json.hpp"

#define OFFBASE_PATH_SEP "/"

/**
 * A super-minimal in-filesystem JSON object persistence store
 */
class offbase : public nlohmann::json
{
public:
	offbase(const char *p) :
		nlohmann::json(nlohmann::json::object()),
		_path(p),
		_saved(nlohmann::json::object())
	{
		this->load();
	}

	~offbase()
	{
		this->commit();
	}

	/**
	 * Load this instance from disk, clearing any existing contents first
	 *
	 * If the 'errors' vector is NULL, false is returned and reading aborts
	 * on any error. If this parameter is non-NULL the paths of errors will
	 * be added to the vector and reading will continue. False will only be
	 * returned on really big errors like no path being defined.
	 *
	 * @param errors If specified, fill this vector with the paths to any objects that fail read
	 * @return True on success, false on fatal error
	 */
	inline bool load(std::vector<std::string> *errors = (std::vector<std::string> *)0)
	{
		if (!_path.length())
			return false;
		*this = nlohmann::json::object();
		if (!_loadObj(_path,*this,errors))
			return false;
		_saved = *(reinterpret_cast<nlohmann::json *>(this));
		return true;
	}

	/**
	 * Commit any pending changes to this object to disk
	 *
	 * @return True on success or false if an I/O error occurred
	 */
	inline bool commit(std::vector<std::string> *errors = (std::vector<std::string> *)0)
	{
		if (!_path.length())
			return false;
		if (!_commitObj(_path,*this,&_saved,errors))
			return false;
		_saved = *(reinterpret_cast<nlohmann::json *>(this));
		return true;
	}

	static inline std::string escapeKey(const std::string &k)
	{
		std::string e;
		const char *ptr = k.data();
		const char *eof = ptr + k.length();
		char tmp[8];
		while (ptr != eof) {
			if ( ((*ptr >= 'a')&&(*ptr <= 'z')) || ((*ptr >= 'A')&&(*ptr <= 'Z')) || ((*ptr >= '0')&&(*ptr <= '9')) || (*ptr == '.') || (*ptr == '_') || (*ptr == '-') || (*ptr == ',') )
				e.push_back(*ptr);
			else {
				snprintf(tmp,sizeof(tmp),"~%.2x",(unsigned int)*ptr);
				e.append(tmp);
			}
			++ptr;
		}
		return e;
	}

	static inline std::string unescapeKey(const std::string &k)
	{
		std::string u;
		const char *ptr = k.data();
		const char *eof = ptr + k.length();
		char tmp[8];
		while (ptr != eof) {
			if (*ptr == '~') {
				if (++ptr == eof) break;
				tmp[0] = *ptr;
				if (++ptr == eof) break;
				tmp[1] = *(ptr++);
				tmp[2] = (char)0;
				u.push_back((char)strtol(tmp,(char **)0,16));
			} else {
				u.push_back(*(ptr++));
			}
		}
		return u;
	}

private:
	static inline bool _readFile(const char *path,std::string &buf)
	{
		char tmp[4096];
		FILE *f = fopen(path,"rb");
		if (f) {
			for(;;) {
				long n = (long)fread(tmp,1,sizeof(tmp),f);
				if (n > 0)
					buf.append(tmp,n);
				else break;
			}
			fclose(f);
			return true;
		}
		return false;
	}

	static inline bool _loadArr(const std::string &path,nlohmann::json &arr,std::vector<std::string> *errors)
	{
		std::map<unsigned long,nlohmann::json> atmp; // place into an ordered container first because filesystem does not guarantee this

		struct dirent dbuf;
		struct dirent *de;
		DIR *d = opendir(path.c_str());
		if (d) {
			while (!readdir_d(d,&dbuf,&de)) {
				if (!de) break;
				const std::string name(de->d_name);
				if (name.length() != 12) continue; // array entries are XXXXXXXXXX.T
				if (name[name.length()-2] == '.') {
					if (name[name.length()-1] == 'V') {
						std::string buf;
						if (_readFile((path + OFFBASE_PATH_SEP + name).c_str(),buf)) {
							try {
								atmp[strtoul(name.substr(0,10).c_str(),(char **)0,16)] = nlohmann::json::parse(buf);
							} catch ( ... ) {
								if (errors) {
									errors->push_back(path + OFFBASE_PATH_SEP + name);
								} else {
									return false;
								}
							}
						} else if (errors) {
							errors->push_back(path + OFFBASE_PATH_SEP + name);
						} else return false;
					} else if (name[name.length()-1] == 'O') {
						if (!_loadObj(path + OFFBASE_PATH_SEP + name,atmp[strtoul(name.substr(0,10).c_str(),(char **)0,16)] = nlohmann::json::object(),errors))
							return false;
					} else if (name[name.length()-1] == 'A') {
						if (!_loadArr(path + OFFBASE_PATH_SEP + name,atmp[strtoul(name.substr(0,10).c_str(),(char **)0,16)] = nlohmann::json::array(),errors))
							return false;
					}
				}
			}
			closedir(d);
		} else if (errors) {
			errors->push_back(path);
		} else return false;

		if (atmp.size() > 0) {
			unsigned long lasti = 0;
			for(std::map<unsigned long,nlohmann::json>::iterator i(atmp.begin());i!=atmp.end();++i) {
				for(unsigned long k=lasti;k<i->first;++k) // fill any gaps with nulls
					arr.push_back(nlohmann::json(std::nullptr_t));
				lasti = i->first;
				arr.push_back(i->second);
			}
		}

		return true;
	}

	static inline bool _loadObj(const std::string &path,nlohmann::json &obj,std::vector<std::string> *errors)
	{
		struct dirent dbuf;
		struct dirent *de;
		DIR *d = opendir(path.c_str());
		if (d) {
			while (!readdir_d(d,&dbuf,&de)) {
				if (!de) break;
				if ((strcmp(de->d_name,".") == 0)||(strcmp(de->d_name,"..") == 0)) continue; // sanity check
				const std::string name(de->d_name);
				if (name.length() <= 2) continue;
				if (name[name.length()-2] == '.') {
					if (name[name.length()-1] == 'V') {
						std::string buf;
						if (_readFile((path + OFFBASE_PATH_SEP + name).c_str(),buf)) {
							try {
								obj[unescapeKey(name)] = nlohmann::json::parse(buf);
							} catch ( ... ) {
								if (errors) {
									errors->push_back(path + OFFBASE_PATH_SEP + name);
								} else {
									return false;
								}
							}
						} else if (errors) {
							errors->push_back(path + OFFBASE_PATH_SEP + name);
						} else return false;
					} else if (name[name.length()-1] == 'O') {
						if (!_loadObj(path + OFFBASE_PATH_SEP + name,obj[unescapeKey(name)] = nlohmann::json::object(),errors))
							return false;
					} else if (name[name.length()-1] == 'A') {
						if (!_loadArr(path + OFFBASE_PATH_SEP + name,obj[unescapeKey(name)] = nlohmann::json::array(),errors))
							return false;
					}
				}
			}
			closedir(d);
		} else if (errors) {
			errors->push_back(path);
		} else return false;
		return true;
	}

	static inline void _rmDashRf(const std::string &path)
	{
		struct dirent dbuf;
		struct dirent *de;
		DIR *d = opendir(path.c_str());
		if (d) {
			while (!readdir_r(d,&dbuf,&de)) {
				if (!de) break;
				if ((strcmp(de->d_name,".") == 0)||(strcmp(de->d_name,"..") == 0)) continue; // sanity check
				const std::string full(path + OFFBASE_PATH_SEP + de->d_name);
				if (unlink(full.c_str())) {
					_rmDashRf(full);
					rmdir(full.c_str());
				}
			}
			closedir(d);
		}
		rmdir(path.c_str());
	}

	static inline bool _commitArr(const std::string &path,const nlohmann::json &arr,const nlohmann::json *previous,std::vector<std::string> *errors)
	{
		char tmp[32];

		if (!arr.is_array())
			return false;

		mkdir(path.c_str(),0755);

		for(unsigned long i=0;i<(unsigned long)arr.size();++i) {
			const nlohmann::json &value = arr[i];

			const nlohmann::json *next = (const nlohmann::json *)0;
			if ((previous)&&(previous->is_array())&&(i < previous->size())) {
				next = &((*previous)[i]);
				if (*next == value)
					continue;
			}

			if (value.is_object()) {
				snprintf(tmp,sizeof(tmp),"%s%.10lx.O",OFFBASE_PATH_SEP,i);
				if (!_commitObj(path + tmp,value,next,errors))
					return false;
				snprintf(tmp,sizeof(tmp),"%s%.10lx.V",OFFBASE_PATH_SEP,i);
				unlink((path + tmp).c_str());
				snprintf(tmp,sizeof(tmp),"%s%.10lx.A",OFFBASE_PATH_SEP,i);
				_rmDashRf(path + tmp);
			} else if (value.is_array()) {
				snprintf(tmp,sizeof(tmp),"%s%.10lx.A",OFFBASE_PATH_SEP,i);
				if (!_commitArr(path + tmp,value,next,errors))
					return false;
				snprintf(tmp,sizeof(tmp),"%s%.10lx.O",OFFBASE_PATH_SEP,i);
				_rmDashRf(path + tmp);
				snprintf(tmp,sizeof(tmp),"%s%.10lx.V",OFFBASE_PATH_SEP,i);
				unlink((path + tmp).c_str());
			} else {
				snprintf(tmp,sizeof(tmp),"%s%.10lx.V",OFFBASE_PATH_SEP,i);
				FILE *f = fopen((path + tmp).c_str(),"w");
				if (f) {
					const std::string v(value.dump());
					if (fwrite(v.c_str(),v.length(),1,f) != 1) {
						fclose(f);
						return false;
					} else {
						fclose(f);
					}
				} else {
					return false;
				}
				snprintf(tmp,sizeof(tmp),"%s%.10lx.A",OFFBASE_PATH_SEP,i);
				_rmDashRf(path + tmp);
				snprintf(tmp,sizeof(tmp),"%s%.10lx.O",OFFBASE_PATH_SEP,i);
				_rmDashRf(path + tmp);
			}
		}

		if ((previous)&&(previous->is_array())) {
			for(unsigned long i=(unsigned long)arr.size();i<(unsigned long)previous->size();++i) {
				snprintf(tmp,sizeof(tmp),"%s%.10lx.V",OFFBASE_PATH_SEP,i);
				unlink((path + tmp).c_str());
				snprintf(tmp,sizeof(tmp),"%s%.10lx.A",OFFBASE_PATH_SEP,i);
				_rmDashRf(path + tmp);
				snprintf(tmp,sizeof(tmp),"%s%.10lx.O",OFFBASE_PATH_SEP,i);
				_rmDashRf(path + tmp);
			}
		}

		return true;
	}

	static inline bool _commitObj(const std::string &path,const nlohmann::json &obj,const nlohmann::json *previous,std::vector<std::string> *errors)
	{
		if (!obj.is_object())
			return false;

		mkdir(path.c_str(),0755);

		for(nlohmann::json::const_iterator i(obj.begin());i!=obj.end();++i) {
			if (i.key().length() == 0)
				continue;

			const nlohmann::json *next = (const nlohmann::json *)0;
			if ((previous)&&(previous->is_object())) {
				nlohmann::json::const_iterator saved(previous->find(i.key()));
				if (saved != previous->end()) {
					next = &(saved.value());
					if (i.value() == *next)
						continue;
				}
			}

			const std::string keyp(path + OFFBASE_PATH_SEP + escapeKey(i.key()));
			if (i.value().is_object()) {
				if (!_commitObj(keyp + ".O",i.value(),next,errors))
					return false;
				unlink((keyp + ".V").c_str());
				_rmDashRf(keyp + ".A");
			} else if (i.value().is_array()) {
				if (!_commitArr(keyp + ".A",i.value(),next,errors))
					return false;
				unlink((keyp + ".V").c_str());
				_rmDashRf(keyp + ".O");
			} else {
				FILE *f = fopen((keyp + ".V").c_str(),"w");
				if (f) {
					const std::string v(i.value().dump());
					if (fwrite(v.c_str(),v.length(),1,f) != 1) {
						fclose(f);
						return false;
					} else {
						fclose(f);
					}
				} else {
					return false;
				}
				_rmDashRf(keyp + ".A");
				_rmDashRf(keyp + ".O");
			}
		}

		if ((previous)&&(previous->is_object())) {
			for(nlohmann::json::const_iterator i(previous->begin());i!=previous->end();++i) {
				if ((i.key().length() > 0)&&(obj.find(i.key()) == obj.end())) {
					const std::string keyp(path + OFFBASE_PATH_SEP + escapeKey(i.key()));
 					unlink((keyp + ".V").c_str());
					_rmDashRf(keyp + ".A");
					_rmDashRf(keyp + ".O");
				}
			}
		}

		return true;
	}

	std::string _path;
	nlohmann::json _saved;
};

#endif
