/*
 * VueceLogger.h
 *
 *  Created on: Oct 31, 2014
 *      Author: jingjing
 */

#ifndef VUECELOGGER_H_
#define VUECELOGGER_H_


class VueceLogger {
public:
//	static void Init(void);
//	static void Deinit(void);
	static void Debug(const char* format, ...);
	static void Info(const char* format, ...);
	static void Warn(const char* format, ...);
	static void Error(const char* format, ...);
	static void Fatal(const char* format, ...);
};


#endif /* VUECELOGGER_H_ */
