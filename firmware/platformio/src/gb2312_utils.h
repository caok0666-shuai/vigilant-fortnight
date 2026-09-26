#ifndef GB2312_UTILS_H
#define GB2312_UTILS_H

#include <Arduino.h>

// ============================================================
// GB2312 编码工具
// SYN6288 需要 GB2312 编码的中文文本
// 此文件提供预定义的常用短语 GB2312 字节数组
// 以及 UTF-8 到 GB2312 的简单转换
// ============================================================

// --- 常用短语 (GB2312 编码) ---

// 欢迎来到社区智能书架
extern const uint8_t GB_WELCOME[];
extern const size_t    GB_WELCOME_LEN;

// 请将书籍放在感应区
extern const uint8_t GB_PLACE_BOOK[];
extern const size_t    GB_PLACE_BOOK_LEN;

// 开始您的阅读之旅
extern const uint8_t GB_START_READING[];
extern const size_t    GB_START_READING_LEN;

// 借书成功
extern const uint8_t GB_BORROW_OK[];
extern const size_t    GB_BORROW_OK_LEN;

// 还书成功
extern const uint8_t GB_RETURN_OK[];
extern const size_t    GB_RETURN_OK_LEN;

// 系统已就绪
extern const uint8_t GB_READY[];
extern const size_t    GB_READY_LEN;

// 请拿好您的书籍
extern const uint8_t GB_TAKE_BOOK[];
extern const size_t    GB_TAKE_BOOK_LEN;

// 祝您阅读愉快
extern const uint8_t GB_ENJOY_READING[];
extern const size_t    GB_ENJOY_READING_LEN;

// 感谢您的归还
extern const uint8_t GB_THANKS_RETURN[];
extern const size_t    GB_THANKS_RETURN_LEN;

// 期待您再次借阅
extern const uint8_t GB_COME_AGAIN[];
extern const size_t    GB_COME_AGAIN_LEN;

// 检测到读者靠近
extern const uint8_t GB_READER_NEAR[];
extern const size_t    GB_READER_NEAR_LEN;

// 社区留言
extern const uint8_t GB_COMMUNITY_MSG[];
extern const size_t    GB_COMMUNITY_MSG_LEN;

// 为您导读
extern const uint8_t GB_GUIDE_READING[];
extern const size_t    GB_GUIDE_READING_LEN;

// --- 辅助函数 ---

// 将 UTF-8 字符串转换为 GB2312 字节数组
// 注意: 仅支持常见汉字，不支持的字符保持原样
// 返回转换后的字节数
size_t utf8_to_gb2312(const char* utf8, uint8_t* gb_buf, size_t buf_size);

// 拼接 GB2312 字节数组并发送到 SYN6288
void gb_speak(const uint8_t* data, size_t len);

#endif
