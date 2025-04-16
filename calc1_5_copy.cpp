#include <iostream>
#include <stack>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <cmath>
#include <functional>
#include <vector>
#include <sstream>

//#define DEBUG

// 枚举类，用于表示值的类型
enum class ValueType { NUMBER, STRING };

// Value 类，用于存储不同类型的值
class Value {
public:
    ValueType type;          // 值的类型
    double numberValue;      // 数值类型的值
    std::string stringValue; // 字符串类型的值

    // 默认构造函数，初始化为数值类型，值为 0.0
    Value() : type(ValueType::NUMBER), numberValue(0.0) {}

    // 构造函数，初始化为数值类型
    Value(double num) : type(ValueType::NUMBER), numberValue(num) {}

    // 构造函数，初始化为字符串类型
    Value(const std::string& str) : type(ValueType::STRING), stringValue(str) {}
};

// Token 类，用于表示表达式中的令牌
class Token {
public:
    // 枚举类型，用于表示令牌的类型
    enum Type { NUMBER, STRING, OPERATOR, FUNCTION, VARIABLE };
    Type type;        // 令牌的类型
    std::string value; // 令牌的值

    // 构造函数，初始化令牌的类型和值
    Token(Type t, const std::string& val) : type(t), value(val) {}
};

using namespace std;

// 全局栈，用于存储操作符
std::stack<Token> operators;

// 全局栈，用于存储后缀表达式的中间结果
std::stack<Token> temp_suffix_result;

// 全局容器，用于存储变量
unordered_map<string, Value> variables;

// 符号结构体，用于存储符号的信息
typedef struct symbol
{
    string name;  // 符号的名称
    string data;  // 符号的数据
    int typ;      // 符号的类型
    int pri;      // 符号的优先级
    int pos;      // 符号的位置
    double val;   // 符号的值
} SYMBOL;

// 清空栈的函数
void clear_stack(stack<Token> &s, stack<Token> &o)
{
    stack<Token> empty; // 创建一个空栈
    s.swap(empty);      // 交换栈 s 和空栈，清空栈 s
    stack<Token> empty2; // 创建另一个空栈
    o.swap(empty2);      // 交换栈 o 和空栈，清空栈 o
}

/*
移除字符串中的空格
参数：
    str: 指向字符串的指针
返回值：
    移除空格后的字符串
*/
string remove_spaces(string *str)
{
    // 使用 std::remove 函数移除字符串中的空格
    str->erase(std::remove(str->begin(), str->end(), ' '), str->end());
    return *str;
}

/*
优先级map
*/
// 操作符优先级映射
std::unordered_map<std::string, int> operatorPrecedence = {
    {"&&", 4},  // 逻辑与
    {"||", 3},  // 逻辑或
    {"->", 2},  // 条件
    {"<->", 1}, // 双条件

    {"+", 11},  // 加法
    {"-", 11},  // 减法
    {"*", 12},  // 乘法
    {"/", 12},  // 除法
    {"**", 13}, // 幂运算
    {"!", 14},  // 逻辑非
    {"==", 8},  // 等于
    {"!=", 8},  // 不等于
    {"<", 9},   // 小于
    {"<=", 9},  // 小于等于
    {">", 9},   // 大于
    {">=", 9}   // 大于等于
};

#ifdef _WIN32
#include <Windows.h>
#include <windows.h>
#endif // _WIN32
double menu(const std::vector<double> &values) {
#ifdef _WIN32
    // 启用 Windows 10 控制台的 ANSI 转义序列处理
    system("chcp 65001 > nul");
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= 0x0004;
    SetConsoleMode(hOut, dwMode);
#endif

    std::cout << "\033[1;34m" << "====================================" << "\033[0m" << std::endl;
    std::cout << "\033[1;34m" << "          计算器功能菜单 📋         " << "\033[0m" << std::endl;
    std::cout << "\033[1;34m" << "====================================" << "\033[0m" << std::endl;

    // 数学功能
    std::cout << "\033[1;33m" << "\n数学功能 📐" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 加法 (+)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 减法 (-)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 乘法 (*)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 除法 (/)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 幂运算 (**)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 正弦函数 sin(x)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 余弦函数 cos(x)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 最大值 max(a, b, ...)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 最小值 min(a, b, ...)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 对数函数 log(value, base)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 求和函数 sum(a, b, ...)" << "\033[0m" << std::endl;
    std::cout << "\033[3;32m" << "➤ 平均值 avg(a, b, ...)" << "\033[0m" << std::endl;

    // 逻辑功能
    std::cout << "\033[1;35m" << "\n逻辑功能 🤔" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 与运算 (&&)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 或运算 (||)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 非运算 (!)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 蕴含 (->)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 等价 (<->)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 等于 (==)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 不等于 (!=)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 小于 (<)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 小于等于 (<=)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 大于 (>)" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 大于等于 (>=)" << "\033[0m" << std::endl;

    std::cout << "\033[1;33m" << "\n离散数学 📖" << "\033[0m" << std::endl;
    std::cout << "\033[3;36m" << "➤ 求真值表和范式  normal_form(string proposition,double mode) " <<std::endl;
    std::cout << "\033[1;33m" << "  例如：normal_form( ( P && Q ) || ( !P && R ), 0)" << "\033[0m" << std::endl; 

    std::cout << "\033[1;34m" << "\n请输入您的表达式：" << "\033[0m" << std::endl;

    return 1;
}
/*
函数名集合
*/
std::unordered_set<std::string> functions = {"max", "min", "log", "sin", "cos"}; // 定义一个包含函数名的集合

/*
处理操作符
*/
// 处理操作符
void process_operator(const std::string &op, std::stack<Token> &operators, std::stack<Token> &temp_suffix_result)
{
    while (!operators.empty()) // 当操作符栈不为空时
    {
        Token topOp = operators.top(); // 获取栈顶操作符

        // 检查栈顶操作符是否为多字符操作符，并找到其优先级
        if (operatorPrecedence.find(topOp.value) != operatorPrecedence.end()) // 如果栈顶操作符在优先级映射中
        {
            int topPrecedence = operatorPrecedence[topOp.value]; // 获取栈顶操作符的优先级
            int currentPrecedence = operatorPrecedence.at(op); // 获取当前操作符的优先级

            // 根据优先级进行比较
            if (topOp.value != "(" && topPrecedence >= currentPrecedence) // 如果栈顶操作符不是左括号且优先级大于等于当前操作符
            {
                temp_suffix_result.push(topOp); // 将栈顶操作符推入后缀表达式结果栈
                operators.pop(); // 弹出栈顶操作符
            }
            else
            {
                break; // 否则，退出循环
            }
        }
        else
        {
            break; // 如果栈顶操作符不在优先级映射中，退出循环
        }
    }
    operators.push(Token(Token::OPERATOR, op)); // 将当前操作符推入操作符栈
}

/*
提示函数。用于提示用户的输入是否规范，或者是否有误。
参数表：
    oriStr: 原始字符串
    Message_level: 提示等级：
        1. "error"：错误提示
        2. "warning"：警告提示
        3. "info"：信息提示
    pos: 在原始字符串中错误/警告/提示的位置
    suggestion: 提示用户可能的替代方案
*/
void Hint(string oriStr, string Message_level, size_t pos, string suggestion = "")
{
    string level_output; // 定义提示等级输出字符串
    string color_code; // 定义颜色代码字符串

    // 根据提示等级设置颜色和提示类型
    if (Message_level == "error") // 如果提示等级为错误
    {
        level_output = "\033[1;31merror\033[0m"; // 设置提示等级输出为红色
        color_code = "\033[1;31m"; // 设置颜色代码为红色
    }
    else if (Message_level == "warning") // 如果提示等级为警告
    {
        level_output = "\033[1;33mwarning\033[0m"; // 设置提示等级输出为黄色
        color_code = "\033[1;33m"; // 设置颜色代码为黄色
    }
    else if (Message_level == "info") // 如果提示等级为信息
    {
        level_output = "\033[1;36minfo\033[0m"; // 设置提示等级输出为天蓝色
        color_code = "\033[1;36m"; // 设置颜色代码为天蓝色
    }

    // 输出提示类型
    cout << level_output << ": at position " << pos << endl; // 输出提示等级和位置

    // 输出带颜色的原始字符串
    cout << "        " << oriStr.substr(0, pos); // 输出错误前的部分，默认颜色
    cout << color_code << oriStr.substr(pos, 1) << "\033[0m"; // 输出错误字符，带颜色
    cout << oriStr.substr(pos + 1) << endl; // 输出错误后面的部分，默认颜色

    // 输出指向错误字符的波浪线，长度和错误字符保持一致
    cout << "        " << string(pos, ' ') << color_code << "^"; // 输出指向错误字符的波浪线
    cout << string(oriStr.length() - pos - 1, '~') << "\033[0m" << endl; // 输出波浪线后面的部分，默认颜色

    // 输出建议替代方案
    if (!suggestion.empty()) // 如果有建议替代方案
    {
        if (Message_level == "error") // 如果提示等级为错误
        {
            cout << "问题:  '\e[1;31m" << suggestion << "\e[0m'" << endl; // 输出建议替代方案，红色
        }
        else if (Message_level == "info") // 如果提示等级为信息
        {
            cout << "可选的建议: '\e[1;32m" << suggestion << "\e[0m'" << endl; // 输出建议替代方案，绿色
        }
        else if (Message_level == "warning") // 如果提示等级为警告
        {
            cout << "警告: '\e[1;33m" << suggestion << "\e[0m'" << endl; // 输出建议替代方案，黄色
        }
    }

    // 如果是 error，终止程序
    if (Message_level == "error") // 如果提示等级为错误
    {
        clear_stack(temp_suffix_result, operators); // 清空栈
        throw std::invalid_argument("本轮循环因异常而终止"); // 抛出异常，终止当前循环
    }
}

// 定义函数类型
using OperatorFunc = std::function<double(double, double)>; // 定义操作符函数类型
using FunctionFunc = std::function<double(double)>; // 定义一元函数类型
using MultiFunctionFunc = std::function<double(const std::vector<double> &)>; // 定义 n 元函数类型

// 定义操作函数(在此添加函数后，记得去函数名map中添加对应名字)
double if_eq(double left, double right) { return left == right; } // 定义等于操作符函数
double if_ne(double left, double right) { return left != right; } // 定义不等于操作符函数
double if_lt(double left, double right) { return left < right; } // 定义小于操作符函数
double if_le(double left, double right) { return left <= right; } // 定义小于等于操作符函数
double if_gt(double left, double right) { return left > right; } // 定义大于操作符函数
double if_ge(double left, double right) { return left >= right; } // 定义大于等于操作符函数
double Not(double left, double right) { return !right; } // 定义逻辑非操作符函数
double And(double left, double right) { return left && right; } // 定义逻辑与操作符函数
double Or(double left, double right) { return left || right; } // 定义逻辑或操作符函数
double implication(double left, double right) { return !left || right; } // 定义条件操作符函数
double equivalence(double left, double right) { return (!left || right) && (left || !right); } // 定义双条件操作符函数

double add(double left, double right) { return left + right; } // 定义加法操作符函数
double subtract(double left, double right) { return left - right; } // 定义减法操作符函数
double multiply(double left, double right) { return left * right; } // 定义乘法操作符函数
double divide(double left, double right) // 定义除法操作符函数
{
    if (right == 0) // 如果除数为 0
    {
        clear_stack(temp_suffix_result, operators); // 清空栈
        throw std::invalid_argument("\033[1;31m除数不能为0\033[0m"); // 抛出异常，提示除数不能为 0
    }
    return left / right; // 返回除法结果
}
double power(double left, double right) { return std::pow(left, right); } // 定义幂运算操作符函数

double sin_func(double x) { return std::sin(x); } // 定义正弦函数
double cos_func(double x) { return std::cos(x); } // 定义余弦函数

// 多元函数
double sum_func(const std::vector<double> &values) // 定义求和函数
{
    double sum = 0; // 初始化和为 0
    for (double value : values) // 遍历向量中的每个元素
    {
        sum += value; // 将每个元素的值累加到 sum 中
    }
    return sum; // 返回计算得到的和
}
double avg_func(const std::vector<double> &values) // 定义求平均值函数
{
    double sum = 0; // 初始化和为 0
    for (double value : values) // 遍历向量中的每个元素
    {
        sum += value; // 将每个元素的值累加到 sum 中
    }
    return sum / values.size(); // 返回计算得到的平均值
}
// 定义各个多元函数
double max_func(const std::vector<double> &args) // 定义求最大值函数
{
    return *std::max_element(args.begin(), args.end()); // 返回向量中的最大值
}

double min_func(const std::vector<double> &args) // 定义求最小值函数
{
    return *std::min_element(args.begin(), args.end()); // 返回向量中的最小值
}

double log_func(const std::vector<double> &args) // 定义对数函数
{
    if (args.size() != 2) // 如果参数个数不等于 2
    {
        throw std::invalid_argument("\033[1;31mlog 函数需要两个参数\033[0m"); // 抛出异常，提示 log 函数需要两个参数
    }
    double value = args[0]; // 获取第一个参数
    double base = args[1]; // 获取第二个参数
    if (base <= 0 || base == 1 || value <= 0) // 如果对数底数小于等于 0 或等于 1 或对数值小于等于 0
    {
        throw std::invalid_argument("\033[1;31m非法的对数参数\033[0m"); // 抛出异常，提示非法的对数参数
    }
    return std::log(value) / std::log(base); // 返回计算得到的对数值
}

// 操作符映射表
std::unordered_map<std::string, OperatorFunc> operatorMap = {
    {"+", add}, // 加法操作符映射
    {"-", subtract}, // 减法操作符映射
    {"*", multiply}, // 乘法操作符映射
    {"/", divide}, // 除法操作符映射
    {"**", power}, // 幂运算操作符映射
    {"==", if_eq}, // 等于操作符映射
    {"!=", if_ne}, // 不等于操作符映射
    {"<", if_lt}, // 小于操作符映射
    {"<=", if_le}, // 小于等于操作符映射
    {">", if_gt}, // 大于操作符映射
    {">=", if_ge}, // 大于等于操作符映射
    {"!", Not}, // 逻辑非操作符映射
    {"&&", And}, // 逻辑与操作符映射
    {"||", Or}, // 逻辑或操作符映射
    {"<->", equivalence}, // 双条件操作符映射
    {"->", implication}}; // 条件操作符映射

// 一元函数映射表
std::unordered_map<std::string, FunctionFunc> functionMap = {
    {"sin", sin_func}, // 正弦函数映射
    {"cos", cos_func}}; // 余弦函数映射
// 多元函数映射表
std::unordered_map<std::string, MultiFunctionFunc> multiFunctionMap = {
    {"max", max_func}, // 最大值函数映射
    {"min", min_func}, // 最小值函数映射
    {"log", log_func}, // 对数函数映射
    {"sum", sum_func}, // 求和函数映射
    {"avg", avg_func}, // 求平均值函数映射
    {"menu", menu}}; // 示例函数名映射

// 多元函数执行器
double executeMultiFunction(const std::string &funcName, const std::vector<double> &args) // 定义多元函数执行器
{
    auto it = multiFunctionMap.find(funcName); // 在多元函数映射表中查找函数名
    if (it != multiFunctionMap.end()) // 如果找到函数名
    {
        return it->second(args); // 执行对应的多元函数，并返回结果
    }
    else
    {
        throw std::invalid_argument("未知的多元函数: " + funcName); // 抛出异常，提示未知的多元函数
    }
}

// 操作符规范替换表
std::unordered_map<std::string, std::string> replacementMap = {
        { R"(^menu$)", "menu(1)" },
    { R"(^help$)", "menu(1)" },
    { R"(^menu\(\)$)", "menu(1)" },
    { R"(^help\(\)$)", "menu(1)" },
    { R"(^/\?$)", "menu(1)" },
    { R"(^\?$)", "menu(1)" },

};

/*
优化原始表达式，方便转化为逆波兰表达式
*/
void Expression_optimization(string *str)
{
    // 遍历字符串，查找并替换大写的函数名为小写
    for (size_t i = 0; i < str->size(); ++i) // 遍历字符串的每个字符
    {
        if (isalpha(str->at(i))) // 如果当前字符是字母
        {
            size_t start = i; // 记录字母的起始位置
            while (i < str->size() && isalpha(str->at(i))) // 遍历连续的字母
            {
                ++i; // 移动到下一个字符
            }
            string token = str->substr(start, i - start); // 提取连续的字母作为一个标记
            string lower_token = token; // 创建一个标记的副本
            transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower); // 将标记转换为小写
            if (functions.find(lower_token) != functions.end()) // 如果小写标记在函数名集合中
            {
                // 只有在函数名包含大写字母时才调用 Hint
                if (token != lower_token) // 如果原始标记和小写标记不同
                {
                    // 提示用户并指明位置
                    Hint(*str, "info", start, lower_token); // 提示用户函数名应为小写
                    str->replace(start, token.size(), lower_token); // 将原始标记替换为小写标记
                }
            }
        }
    }

    // 使用替换表进行替换
    for (const auto &pair : replacementMap) // 遍历替换表中的每个键值对
    {
        std::regex pattern(pair.first); // 创建正则表达式模式
        std::string replacement = pair.second; // 获取替换字符串
        std::smatch match; // 用于存储匹配结果
        std::string temp_str = *str; // 创建字符串的副本
        while (std::regex_search(temp_str, match, pattern)) // 查找匹配的模式
        {
            size_t pos = match.position(); // 获取匹配的位置
            Hint(*str, "info", pos, replacement); // 提示用户替换信息
            *str = std::regex_replace(*str, pattern, replacement); // 进行替换
            temp_str = *str; // 更新字符串副本
        }
    }

    // 检查并添加缺失的乘号
    for (size_t i = 0; i < str->size(); ++i) // 遍历字符串的每个字符
    {
        if (isdigit(str->at(i))) // 如果当前字符是数字
        {
            // 数字与括号间
            if (i + 1 < str->size() && str->at(i + 1) == '(') // 如果数字后面是左括号
            {
                str->insert(i + 1, "*"); // 在数字和括号之间插入乘号
                Hint(*str, "info", i + 1, "*"); // 提示用户插入了乘号
            }
            // 数字与变量间或数字与函数间
            if (i + 1 < str->size() && isalpha(str->at(i + 1))) // 如果数字后面是字母
            {
                str->insert(i + 1, "*"); // 在数字和字母之间插入乘号
                Hint(*str, "info", i + 1, "*"); // 提示用户插入了乘号
            }
        }
        // 括号与变量间或括号与函数间
        if (str->at(i) == ')' && i + 1 < str->size() && (isalpha(str->at(i + 1)) || str->at(i + 1) == '(')) // 如果右括号后面是字母或左括号
        {
            str->insert(i + 1, "*"); // 在括号之间插入乘号
            Hint(*str, "info", i + 1, "*"); // 提示用户插入了乘号
        }
        // 相反括号之间
        if (str->at(i) == ')' && i + 1 < str->size() && str->at(i + 1) == '(') // 如果右括号后面是左括号
        {
            str->insert(i + 1, "*"); // 在括号之间插入乘号
            Hint(*str, "info", i + 1, "*"); // 提示用户插入了乘号
        }
    }

    // 在单独的负数，负号前面插入0
#ifdef DEBUG
    cout << "ori_Expression_length: " << str->size() << endl; // 输出原始表达式的长度
#endif

    for (int i = 0; i < str->size(); i++) // 遍历字符串的每个字符
    {
        if (str->at(i) == '-' && (i == 0 || str->at(i - 1) == '(')) // 如果当前字符是负号且在字符串开头或左括号后面
        {
            str->insert(i, "0"); // 在负号前面插入0
        }
    }
    for (int i = 0; i < str->size(); i++) // 遍历字符串的每个字符
    {
        if (str->at(i) == '!') // 如果当前字符是非运算符
        {
            str->insert(i, "1"); // 在非运算符前面插入1
            i++; // 跳过插入的字符
        }
    }

#ifdef DEBUG
    cout << "Expression_optimization: " << *str << endl; // 输出优化后的表达式
#endif
}

// 获取操作符映射表中最长操作符的长度
int get_max_operator_length(const std::unordered_map<std::string, int> &opMap)
{
    int maxLen = 0; // 初始化最大长度为 0
    for (const auto &pair : opMap) // 遍历操作符映射表中的每个键值对
    {
        maxLen = std::max(maxLen, static_cast<int>(pair.first.size())); // 更新最大长度
    }
    return maxLen; // 返回最大长度
}

// 计算两个字符串之间的 Levenshtein 距离
int levenshteinDistance(const std::string &s1, const std::string &s2)
{
    const size_t len1 = s1.size(), len2 = s2.size(); // 获取两个字符串的长度
    std::vector<std::vector<size_t>> d(len1 + 1, std::vector<size_t>(len2 + 1)); // 创建二维数组用于存储距离

    for (size_t i = 0; i <= len1; ++i) // 初始化第一列
        d[i][0] = i;
    for (size_t i = 0; i <= len2; ++i) // 初始化第一行
        d[0][i] = i;

    for (size_t i = 1; i <= len1; ++i) // 遍历第一个字符串的每个字符
        for (size_t j = 1; j <= len2; ++j) // 遍历第二个字符串的每个字符
            d[i][j] = std::min({
                d[i - 1][j] + 1,                                   // 删除
                d[i][j - 1] + 1,                                   // 插入
                d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1) // 替换
            });

    return d[len1][len2]; // 返回两个字符串之间的 Levenshtein 距离
}

// 模糊匹配函数名，返回最接近的函数名
std::string fuzzyMatchFunction(const std::string &current,
                               const std::unordered_map<std::string, FunctionFunc> &functionMap,
                               const std::unordered_map<std::string, MultiFunctionFunc> &multiFunctionMap)
{
    std::string bestMatch; // 初始化最佳匹配的函数名
    int bestDistance = INT_MAX; // 初始化最佳距离为最大整数值

    // 遍历一元函数映射表
    for (const auto &pair : functionMap) // 遍历一元函数映射表中的每个键值对
    {
        int distance = levenshteinDistance(current, pair.first); // 计算当前函数名与映射表中函数名的 Levenshtein 距离
        if (distance < bestDistance) // 如果当前距离小于最佳距离
        {
            bestDistance = distance; // 更新最佳距离
            bestMatch = pair.first; // 更新最佳匹配的函数名
        }
    }

    // 遍历多元函数映射表
    for (const auto &pair : multiFunctionMap) // 遍历多元函数映射表中的每个键值对
    {
        int distance = levenshteinDistance(current, pair.first); // 计算当前函数名与映射表中函数名的 Levenshtein 距离
        if (distance < bestDistance) // 如果当前距离小于最佳距离
        {
            bestDistance = distance; // 更新最佳距离
            bestMatch = pair.first; // 更新最佳匹配的函数名
        }
    }

    return bestMatch; // 返回最佳匹配的函数名
}

void lexer(std::string *str)
{
    remove_spaces(str); // 移除字符串中的空格
    int i = 0; // 初始化索引 i 为 0
    bool lastWasOperatorOrOpenParenthesis = true; // 用于跟踪上一个字符是否为操作符或 '('

    // 获取最长操作符的长度
    int maxLen = get_max_operator_length(operatorPrecedence); // 获取操作符映射表中最长操作符的长度
#ifdef DEBUG
    std::cout << "Max operator length: " << maxLen << std::endl; // 输出最长操作符的长度
#endif

    while (i < str->size()) // 遍历字符串的每个字符
    {
        char current = str->at(i); // 获取当前字符

        if (isdigit(current)) // 如果当前字符是数字
        {
            std::string temp; // 临时字符串用于存储数字
            while (i < str->size() && (isdigit(str->at(i)) || str->at(i) == '.')) // 处理连续的数字
            {
                temp.push_back(str->at(i)); // 将当前字符添加到临时字符串中
                i++; // 移动到下一个字符
            }
            temp_suffix_result.push(Token(Token::NUMBER, temp)); // 将数字作为 Token 推入结果栈
            lastWasOperatorOrOpenParenthesis = false; // 更新标志，表示上一个字符不是操作符或 '('
        }
        else if (isalpha(current)) // 如果当前字符是字母
        {
            std::string temp; // 临时字符串用于存储变量名或函数名
            while (i < str->size() && isalnum(str->at(i))) // 处理连续的字母和数字
            {
                temp.push_back(str->at(i)); // 将当前字符添加到临时字符串中
                i++; // 移动到下一个字符
            }
            // 处理函数名或变量名
            if (functionMap.find(temp) != functionMap.end() || multiFunctionMap.find(temp) != multiFunctionMap.end()) // 如果是函数名
            {
                operators.push(Token(Token::FUNCTION, temp)); // 将函数名作为 Token 推入操作符栈
            }
            else // 如果是变量名
            {
                temp_suffix_result.push(Token(Token::VARIABLE, temp)); // 将变量名作为 Token 推入结果栈
            }
            lastWasOperatorOrOpenParenthesis = false; // 更新标志，表示上一个字符不是操作符或 '('
        }
        else if (current == '(') // 如果当前字符是左括号
        {
            operators.push(Token(Token::OPERATOR, "(")); // 将左括号作为 Token 推入操作符栈
            lastWasOperatorOrOpenParenthesis = true; // 更新标志，表示上一个字符是操作符或 '('
            i++; // 移动到下一个字符
        }
        else if (current == ')') // 如果当前字符是右括号
        {
            while (!operators.empty() && operators.top().value != "(") // 将操作符栈中的操作符推入结果栈，直到遇到左括号
            {
                temp_suffix_result.push(operators.top()); // 将操作符推入结果栈
                operators.pop(); // 弹出操作符栈顶的操作符
            }
            if (!operators.empty() && operators.top().value == "(") // 如果操作符栈顶是左括号
            {
                operators.pop(); // 移除左括号
            }
            if (!operators.empty() && operators.top().type == Token::FUNCTION) // 如果操作符栈顶是函数
            {
                temp_suffix_result.push(operators.top()); // 将函数推入结果栈
                operators.pop(); // 弹出操作符栈顶的函数
            }
            lastWasOperatorOrOpenParenthesis = false; // 更新标志，表示上一个字符不是操作符或 '('
            i++; // 移动到下一个字符
        }
        else if (current == ',') // 如果当前字符是逗号（函数参数分隔符）
        {
            while (!operators.empty() && operators.top().value != "(") // 将操作符栈中的操作符推入结果栈，直到遇到左括号
            {
                temp_suffix_result.push(operators.top()); // 将操作符推入结果栈
                operators.pop(); // 弹出操作符栈顶的操作符
            }
            i++; // 移动到下一个字符
        }
        else if (current == '"') // 如果当前字符是双引号（字符串字面量）
        {
            std::string temp; // 临时字符串用于存储字符串字面量
            i++; // 跳过起始的双引号
            while (i < str->size() && str->at(i) != '"') // 处理字符串字面量
            {
                temp.push_back(str->at(i)); // 将当前字符添加到临时字符串中
                i++; // 移动到下一个字符
            }
            if (i < str->size() && str->at(i) == '"') // 如果遇到结束的双引号
            {
                i++; // 跳过结束的双引号
                temp_suffix_result.push(Token(Token::STRING, temp)); // 将字符串字面量作为 Token 推入结果栈
            }
            else // 如果缺少结束的双引号
            {
                Hint(*str, "error", i, "缺少结束引号"); // 提示用户缺少结束引号
                return; // 终止处理
            }
            lastWasOperatorOrOpenParenthesis = false; // 更新标志，表示上一个字符不是操作符或 '('
        }
        else // 处理操作符
        {
            std::string op; // 临时字符串用于存储操作符
            bool foundOp = false; // 标志用于指示是否找到操作符

            for (int len = maxLen; len >= 1; --len) // 从最长操作符开始查找
            {
                if (i + len <= str->size()) // 如果剩余字符长度足够
                {
                    op = str->substr(i, len); // 提取操作符
                    if (operatorPrecedence.find(op) != operatorPrecedence.end()) // 如果找到操作符
                    {
                        foundOp = true; // 更新标志，表示找到操作符
                        break; // 退出循环
                    }
                }
            }

            if (foundOp) // 如果找到操作符
            {
                if (lastWasOperatorOrOpenParenthesis) // 如果上一个字符是操作符或 '('
                {
                    if (op == "-") // 如果操作符是负号
                    {
                        temp_suffix_result.push(Token(Token::NUMBER, "0")); // 在负号前加一个零
                    }
                    else if (op == "=" && op != "==") // 如果操作符是赋值操作符且不是等于操作符
                    {
                        clear_stack(temp_suffix_result, operators); // 清空栈
                        Hint(*str, "error", i, "赋值操作符 '=' 不能出现在这里"); // 提示用户赋值操作符不能出现在这里
                        return; // 终止处理
                    }
                    else // 其他错误的操作符
                    {
                        clear_stack(temp_suffix_result, operators); // 清空栈
                        Hint(*str, "error", i, "错误的操作符"); // 提示用户错误的操作符
                        return; // 终止处理
                    }
                }
                process_operator(op, operators, temp_suffix_result); // 处理操作符
                lastWasOperatorOrOpenParenthesis = false; // 更新标志，表示上一个字符不是操作符或 '('
                i += op.length(); // 移动索引 i 到操作符之后
            }
            else // 如果未找到操作符
            {
                clear_stack(temp_suffix_result, operators); // 清空栈
                Hint(*str, "error", i, "未知的字符"); // 提示用户未知的字符
                return; // 终止处理
            }
        }
    }

    while (!operators.empty()) // 将操作符栈中的所有操作符推入结果栈
    {
        temp_suffix_result.push(operators.top()); // 将操作符推入结果栈
        operators.pop(); // 弹出操作符栈顶的操作符
    }
}

// 辅助函数：从 Token 获取 Value
Value getValueFromToken(const Token &token, const string &str)
{
    if (token.type == Token::NUMBER) // 如果 Token 类型是数字
    {
        return Value(std::stod(token.value)); // 将 Token 的值转换为 double 并返回
    }
    else if (token.type == Token::STRING) // 如果 Token 类型是字符串
    {
        return Value(token.value); // 返回 Token 的字符串值
    }
    else if (token.type == Token::VARIABLE) // 如果 Token 类型是变量
    {
        auto it = variables.find(token.value); // 在变量映射表中查找变量
        if (it != variables.end()) // 如果找到变量
        {
            return it->second; // 返回变量的值
        }
        else // 如果未找到变量
        {
            size_t pos = str.find(token.value); // 查找变量在字符串中的位置
            Hint(str, "error", pos != string::npos ? pos : 0, "变量 '" + token.value + "' 未定义"); // 提示用户变量未定义
            throw std::invalid_argument("变量 '" + token.value + "' 未定义"); // 抛出异常，提示变量未定义
        }
    }
    else // 如果 Token 类型无效
    {
        size_t pos = str.find(token.value); // 查找无效标记在字符串中的位置
        Hint(str, "error", pos != string::npos ? pos : 0, "无效的标记类型"); // 提示用户无效的标记类型
        throw std::invalid_argument("无效的标记类型"); // 抛出异常，提示无效的标记类型
    }
}

// 修改后的 Binary_Computing_Executor 函数
Value Binary_Computing_Executor(const Value &left, const Value &right, const std::string &op)
{
    if (op == "+") // 如果操作符是加号
    {
        if (left.type == ValueType::NUMBER && right.type == ValueType::NUMBER) // 如果两个操作数都是数字
        {
            return Value(left.numberValue + right.numberValue); // 返回两个数字的和
        }
        else if (left.type == ValueType::STRING && right.type == ValueType::STRING) // 如果两个操作数都是字符串
        {
            return Value(left.stringValue + right.stringValue); // 返回两个字符串的连接
        }
        else // 如果操作数类型不一致
        {
            throw std::invalid_argument("类型错误: '+' 操作符要求操作数类型一致"); // 抛出异常，提示类型错误
        }
    }
    else if (op == "==") // 如果操作符是等于号
    {
        if (left.type == right.type) // 如果两个操作数类型相同
        {
            if (left.type == ValueType::NUMBER) // 如果两个操作数都是数字
            {
                return Value(left.numberValue == right.numberValue ? 1.0 : 0.0); // 返回比较结果
            }
            else if (left.type == ValueType::STRING) // 如果两个操作数都是字符串
            {
                return Value(left.stringValue == right.stringValue ? 1.0 : 0.0); // 返回比较结果
            }
        }
        else // 如果两个操作数类型不同
        {
            return Value(0.0); // 返回 0，表示不相等
        }
    }
    else // 处理其他操作符
    {
        if (left.type == ValueType::NUMBER && right.type == ValueType::NUMBER) // 如果两个操作数都是数字
        {
            auto it = operatorMap.find(op); // 在操作符映射表中查找操作符
            if (it != operatorMap.end()) // 如果找到操作符
            {
                double result = it->second(left.numberValue, right.numberValue); // 执行操作符对应的函数
                return Value(result); // 返回计算结果
            }
            else // 如果未找到操作符
            {
                throw std::invalid_argument("未知的运算符: " + op); // 抛出异常，提示未知的运算符
            }
        }
        else // 如果操作数类型不一致
        {
            throw std::invalid_argument("类型错误: 操作符 '" + op + "' 需要数值类型操作数"); // 抛出异常，提示类型错误
        }
    }
}

// 一元运算执行器
double Unary_Computing_Executor(double value, const std::string func)
{
    auto it = functionMap.find(func); // 在函数映射表中查找函数名
    if (it != functionMap.end()) // 如果找到函数名
    {
        return it->second(value); // 执行函数并返回结果
    }
    clear_stack(temp_suffix_result, operators); // 清空栈
    throw std::invalid_argument("未知的函数: " + func); // 抛出异常，提示未知的函数
}

// 定义允许传入未定义变量的函数集合
std::unordered_set<std::string> neednt_args_func = {"func1", "func2"}; // 示例函数名

// 完整的 calculate 函数
Value calculate(string *str, stack<Token> temp_suffix_result)
{
    std::stack<Token> temp_result, temp_suffix; // 定义两个栈用于存储中间结果和后缀表达式

    // 将 temp_suffix_result 逆序放入 temp_suffix 中
    while (!temp_suffix_result.empty()) // 当 temp_suffix_result 不为空时
    {
        temp_suffix.push(temp_suffix_result.top()); // 将 temp_suffix_result 的栈顶元素推入 temp_suffix 中
        temp_suffix_result.pop(); // 弹出 temp_suffix_result 的栈顶元素
    }

    while (!temp_suffix.empty()) // 当 temp_suffix 不为空时
    {
        Token current = temp_suffix.top(); // 获取 temp_suffix 的栈顶元素
        temp_suffix.pop(); // 弹出 temp_suffix 的栈顶元素

        if (current.type == Token::NUMBER || current.type == Token::STRING || current.type == Token::VARIABLE) // 如果当前标记是数字、字符串或变量
        {
            temp_result.push(current); // 将当前标记推入 temp_result 中
        }
        else if (current.type == Token::OPERATOR) // 如果当前标记是操作符
        {
            if (current.value == "=") // 如果操作符是赋值操作符
            {
                // 处理赋值操作符
                if (temp_result.size() < 2) // 如果 temp_result 中的元素少于 2 个
                {
                    Hint(*str, "error", str->find(current.value), "无效的表达式: 赋值缺少参数"); // 提示用户赋值缺少参数
                    clear_stack(temp_suffix_result, operators); // 清空栈
                    return Value(); // 返回空值
                }
                Token rhsToken = temp_result.top(); // 获取右操作数
                temp_result.pop(); // 弹出右操作数
                Token lhsToken = temp_result.top(); // 获取左操作数
                temp_result.pop(); // 弹出左操作数

                if (lhsToken.type != Token::VARIABLE) // 如果左操作数不是变量
                {
                    Hint(*str, "error", str->find(lhsToken.value), "无效的变量名"); // 提示用户无效的变量名
                    clear_stack(temp_suffix_result, operators); // 清空栈
                    return Value(); // 返回空值
                }
                std::string var_name = lhsToken.value; // 获取变量名

                // 获取 RHS 的值
                Value rhsValue = getValueFromToken(rhsToken, *str); // 获取右操作数的值

                variables[var_name] = rhsValue; // 将变量名和值存储到变量映射表中
                temp_result.push(rhsToken); // 将 RHS 推回栈中
            }
            else // 处理其他操作符
            {
                if (temp_result.size() < 2) // 如果 temp_result 中的元素少于 2 个
                {
                    Hint(*str, "error", str->find(current.value), "无效的表达式: 操作符缺少参数"); // 提示用户操作符缺少参数
                    clear_stack(temp_suffix_result, operators); // 清空栈
                    return Value(); // 返回空值
                }
                Token rightToken = temp_result.top(); // 获取右操作数
                temp_result.pop(); // 弹出右操作数
                Token leftToken = temp_result.top(); // 获取左操作数
                temp_result.pop(); // 弹出左操作数

                Value leftValue = getValueFromToken(leftToken, *str); // 获取左操作数的值
                Value rightValue = getValueFromToken(rightToken, *str); // 获取右操作数的值

                // 执行操作并进行类型检查
                Value resultValue;
                try
                {
                    resultValue = Binary_Computing_Executor(leftValue, rightValue, current.value); // 执行二元运算
                }
                catch (const std::invalid_argument &e) // 捕获异常
                {
                    size_t pos = str->find(current.value); // 查找操作符在字符串中的位置
                    Hint(*str, "error", pos != string::npos ? pos : 0, e.what()); // 提示用户错误信息
                    clear_stack(temp_suffix_result, operators); // 清空栈
                    return Value(); // 返回空值
                }

                // 根据结果类型，创建相应的 Token
                if (resultValue.type == ValueType::NUMBER) // 如果结果类型是数字
                {
                    temp_result.push(Token(Token::NUMBER, std::to_string(resultValue.numberValue))); // 将结果推入 temp_result 中
                }
                else if (resultValue.type == ValueType::STRING) // 如果结果类型是字符串
                {
                    temp_result.push(Token(Token::STRING, resultValue.stringValue)); // 将结果推入 temp_result 中
                }
            }
        }
        else if (current.type == Token::FUNCTION) // 如果当前标记是函数
        {
            // 处理函数调用
            // 检查函数是否存在
            if (functionMap.find(current.value) == functionMap.end() && multiFunctionMap.find(current.value) == multiFunctionMap.end()) // 如果函数不存在
            {
                size_t pos = str->find(current.value); // 查找函数名在字符串中的位置
                Hint(*str, "error", pos != string::npos ? pos : 0, "未知的函数 '" + current.value + "'"); // 提示用户未知的函数
                clear_stack(temp_suffix_result, operators); // 清空栈
                return Value(); // 返回空值
            }

            if (temp_result.empty()) // 如果 temp_result 为空
            {
                Hint(*str, "error", str->find(current.value), "无效的表达式: 函数缺少参数"); // 提示用户函数缺少参数
                clear_stack(temp_suffix_result, operators); // 清空栈
                return Value(); // 返回空值
            }

            // 获取函数参数
            std::vector<double> args; // 定义一个向量用于存储参数
            // 从栈中获取所有参数
            while (!temp_result.empty() && (temp_result.top().type == Token::NUMBER || temp_result.top().type == Token::VARIABLE)) // 当 temp_result 不为空且栈顶元素是数字或变量时
            {
                Token argToken = temp_result.top(); // 获取参数
                temp_result.pop(); // 弹出参数

                Value argValue = getValueFromToken(argToken, *str); // 获取参数的值
                if (argValue.type == ValueType::NUMBER) // 如果参数类型是数字
                {
                    args.push_back(argValue.numberValue); // 将参数值添加到向量中
                }
                else // 如果参数类型不是数字
                {
                    Hint(*str, "error", str->find(argToken.value), "函数参数必须是数字"); // 提示用户函数参数必须是数字
                    clear_stack(temp_suffix_result, operators); // 清空栈
                    return Value(); // 返回空值
                }
            }

            // 反转参数顺序，因为从栈中弹出的参数是逆序的
            std::reverse(args.begin(), args.end()); // 反转向量中的元素顺序

            // 检查函数类型并执行
            if (functionMap.find(current.value) != functionMap.end()) // 如果函数是单参数函数
            {
                if (args.size() != 1) // 如果参数个数不等于 1
                {
                    Hint(*str, "error", str->find(current.value), "函数 '" + current.value + "' 需要一个参数"); // 提示用户函数需要一个参数
                    clear_stack(temp_suffix_result, operators); // 清空栈
                    return Value(); // 返回空值
                }
                double resultValue = Unary_Computing_Executor(args[0], current.value); // 执行单参数函数
                temp_result.push(Token(Token::NUMBER, std::to_string(resultValue))); // 将结果推入 temp_result 中
            }
            else if (multiFunctionMap.find(current.value) != multiFunctionMap.end()) // 如果函数是多参数函数
            {
                double resultValue = executeMultiFunction(current.value, args); // 执行多参数函数
                temp_result.push(Token(Token::NUMBER, std::to_string(resultValue))); // 将结果推入 temp_result 中
            }
        }
        else // 如果标记类型未知
        {
            Hint(*str, "error", str->find(current.value), "未知的标记类型"); // 提示用户未知的标记类型
            clear_stack(temp_suffix_result, operators); // 清空栈
            return Value(); // 返回空值
        }
    }

    // 最终结果
    if (temp_result.size() != 1) // 如果 temp_result 中的元素个数不等于 1
    {
        Hint(*str, "error", 0, "计算错误"); // 提示用户计算错误
        return Value(); // 返回空值
    }

    Token resultToken = temp_result.top(); // 获取结果标记

    // 获取最终结果的 Value
    Value finalValue = getValueFromToken(resultToken, *str); // 获取结果标记的值

    return finalValue; // 返回最终结果
}

void create_variable(string var_name, string expression)
{
    if (isdigit(var_name[0]))
    {
        clear_stack(temp_suffix_result, operators);
        Hint(var_name, "error", 0, "变量名不能以数字开头");
        return;
    }

    if (functions.find(var_name) != functions.end())
    {
        clear_stack(temp_suffix_result, operators);
        Hint(var_name, "error", 0, "变量名不能与函数名重名");
        return;
    }

    lexer(&expression);
    Value result = calculate(&expression, temp_suffix_result);
    variables[var_name] = result;

    // 根据结果类型输出变量和值
    if (result.type == ValueType::NUMBER)
    {
        std::cout << var_name << " = " << result.numberValue << std::endl;
    }
    else if (result.type == ValueType::STRING)
    {
        std::cout << var_name << " = " << result.stringValue << std::endl;
    }
}
int normal_form(std::string proposition, double mode);
void executer(string *str, SYMBOL *var)
{
    Expression_optimization(str);

    // 检查是否有赋值操作 '='
    size_t equal_pos = str->find('=');
    if (equal_pos != string::npos && (equal_pos == 0 || str->at(equal_pos - 1) != '<' && str->at(equal_pos - 1) != '>' && str->at(equal_pos - 1) != '!' && str->at(equal_pos + 1) != '='))
    {
        string var_name = str->substr(0, equal_pos);
        string expression = str->substr(equal_pos + 1);

        // 去除变量名和表达式前后的空白字符
        var_name.erase(var_name.find_last_not_of(" \n\r\t")+1);
        var_name.erase(0, var_name.find_first_not_of(" \n\r\t"));
        expression.erase(expression.find_last_not_of(" \n\r\t")+1);
        expression.erase(0, expression.find_first_not_of(" \n\r\t"));

        // 确保变量名有效
        if (var_name.empty() || !std::isalpha(var_name[0]) || !std::all_of(var_name.begin(), var_name.end(), [](char c){ return std::isalnum(c) || c == '_'; }))
        {
            Hint(*str, "error", equal_pos, "executer报错：无效的变量名");
            return;
        }

        // 检查变量名是否与函数名冲突
        if (functions.find(var_name) != functions.end() || functionMap.find(var_name) != functionMap.end() || multiFunctionMap.find(var_name) != multiFunctionMap.end())
        {
            Hint(*str, "error", equal_pos, "变量名不能与函数名重名");
            return;
        }

        // 处理变量赋值
        lexer(&expression);
        Value result = calculate(&expression, temp_suffix_result);
        variables[var_name] = result;

        // 根据结果类型输出变量和值
        if (result.type == ValueType::NUMBER)
        {
            std::cout << var_name << " = " << result.numberValue << std::endl;
        }
        else if (result.type == ValueType::STRING)
        {
            std::cout << var_name << " = " << result.stringValue << std::endl;
        }
    }
    else
    {
        // 去除表达式前后的空白字符
        str->erase(str->find_last_not_of(" \n\r\t")+1);
        str->erase(0, str->find_first_not_of(" \n\r\t"));

        // 检查是否调用了 normal_form 函数
        if (str->substr(0, 11) == "normal_form")
        {
            // 提取函数参数
            size_t start_pos = str->find("(");
            size_t end_pos = str->find_last_of(")");
            if (start_pos != string::npos && end_pos != string::npos && end_pos > start_pos)
            {
                string args_str = str->substr(start_pos + 1, end_pos - start_pos - 1);
                // 分割参数
                size_t comma_pos = args_str.find(",");
                if (comma_pos != string::npos)
                {
                    string proposition = args_str.substr(0, comma_pos);
                    string mode_str = args_str.substr(comma_pos + 1);
                    // 去除参数前后的引号和空格
                    proposition.erase(0, proposition.find_first_not_of(" \n\r\t\""));
                    proposition.erase(proposition.find_last_not_of(" \n\r\t\"")+1);
                    mode_str.erase(0, mode_str.find_first_not_of(" \n\r\t"));
                    mode_str.erase(mode_str.find_last_not_of(" \n\r\t")+1);
                    double mode = std::stod(mode_str);
                    // 调用 normal_form 函数
                    int result = normal_form(proposition, mode);
                    #ifdef DEBUG
                    std::cout << "normal_form 返回值: " << result << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "normal_form 函数参数错误" << std::endl;
                }
            }
            else
            {
                std::cerr << "normal_form 函数格式错误" << std::endl;
            }
        }
        else
        {
            lexer(str);
            Value result = calculate(str, temp_suffix_result);

            // 根据结果类型输出结果
            if (result.type == ValueType::NUMBER)
            {
                std::cout << result.numberValue << std::endl;
            }
            else if (result.type == ValueType::STRING)
            {
                std::cout << result.stringValue << std::endl;
            }
        }
    }

    // 清空栈
    std::stack<Token> empty;
    temp_suffix_result.swap(empty);
    std::stack<Token> empty2;
    operators.swap(empty2);
}

// 辅助函数：拼接字符串
std::string join(const std::string &separator, const std::vector<std::string> &elements)
{
    std::string result;
    for (size_t i = 0; i < elements.size(); ++i)
    {
        result += elements[i];
        if (i != elements.size() - 1)
        {
            result += separator;
        }
    }
    return result;
}
/* 添加 normal_form 函数 */
/* 添加 normal_form 函数 */
int normal_form(std::string proposition, double mode)
{
    // 提取命题变元
    std::unordered_set<std::string> vars_set; // 定义一个集合用于存储命题变元
    size_t i = 0; // 初始化索引 i 为 0
    while (i < proposition.size()) // 遍历命题字符串的每个字符
    {
        if (isalpha(proposition[i])) // 如果当前字符是字母
        {
            std::string var; // 定义一个字符串用于存储命题变元
            while (i < proposition.size() && isalnum(proposition[i])) // 处理连续的字母和数字
            {
                var += proposition[i]; // 将当前字符添加到命题变元中
                i++; // 移动到下一个字符
            }
            vars_set.insert(var); // 将命题变元添加到集合中
        }
        else // 如果当前字符不是字母
        {
            i++; // 移动到下一个字符
        }
    }
    std::vector<std::string> vars(vars_set.begin(), vars_set.end()); // 将集合转换为向量
    std::sort(vars.begin(), vars.end()); // 对向量进行排序
    size_t n = vars.size(); // 获取命题变元的数量

    // 存储真值表
    std::vector<std::vector<bool>> truth_table; // 定义一个二维向量用于存储真值表
    std::vector<bool> results; // 定义一个向量用于存储命题的真值

    // 打印表头
    std::cout << "\033[1;33m"; // 设置黄色字体
    for (const auto &var : vars) // 遍历命题变元
    {
        std::cout << var << "\t"; // 打印命题变元
    }
    std::cout << proposition << "\033[0m" << std::endl; // 打印命题并重置颜色

    // 枚举所有可能的真值组合
    for (size_t i = 0; i < (1 << n); ++i) // 遍历所有可能的真值组合
    {
        // 设置命题变元的真值
        std::unordered_map<std::string, Value> local_variables; // 定义一个映射用于存储局部变量
        std::vector<bool> row_values; // 定义一个向量用于存储当前行的真值

        for (size_t j = 0; j < n; ++j) // 遍历命题变元
        {
            bool value = (i >> (n - j - 1)) & 1; // 计算当前命题变元的真值
            local_variables[vars[j]] = Value(value ? 1.0 : 0.0); // 将真值存储到局部变量映射中
            row_values.push_back(value); // 将真值添加到当前行的真值向量中
        }

        // 设置全局变量用于计算
        variables = local_variables; // 将局部变量映射赋值给全局变量映射

        // 计算命题的值
        std::string temp_prop = proposition; // 创建命题的副本
        clear_stack(temp_suffix_result, operators); // 清空栈
        Expression_optimization(&temp_prop); // 优化表达式
        lexer(&temp_prop); // 词法分析
        Value result; // 定义一个变量用于存储计算结果
        try
        {
            result = calculate(&temp_prop, temp_suffix_result); // 计算命题的值
        }
        catch (const std::invalid_argument &e) // 捕获异常
        {
            // 处理计算过程中的异常
            clear_stack(temp_suffix_result, operators); // 清空栈
            std::cerr << e.what() << std::endl; // 输出异常信息
            return -1; // 返回错误码
        }

        // 获取命题的真值
        bool prop_value = (result.numberValue != 0.0); // 判断命题的真值
        truth_table.push_back(row_values); // 将当前行的真值添加到真值表中
        results.push_back(prop_value); // 将命题的真值添加到结果向量中

        // 判断是否需要高亮显示
        bool highlight = false; // 初始化高亮标志为 false
        if (mode == 0) // 如果模式为 0
        {
            // 主合取范式，命题为假时高亮
            highlight = !prop_value; // 如果命题为假，则高亮显示
        }
        else // 如果模式不为 0
        {
            // 主析取范式，命题为真时高亮
            highlight = prop_value; // 如果命题为真，则高亮显示
        }

        // 打印真值表的每一行
        if (highlight) // 如果需要高亮显示
        {
            std::cout << "\033[42m"; // 设置绿色背景
        }

        // 打印变量的真值
        for (bool val : row_values) // 遍历当前行的真值
        {
            if (val) // 如果真值为真
            {
                std::cout << "\033[1;32mT\033[0m\t"; // 绿色字体 T，重置字体颜色
            }
            else // 如果真值为假
            {
                std::cout << "\033[1;31mF\033[0m\t"; // 红色字体 F，重置字体颜色
            }

            if (highlight) // 如果需要高亮显示
            {
                std::cout << "\033[42m"; // 重新设置背景色，保持背景
            }
        }

        // 打印命题的真值
        if (prop_value) // 如果命题为真
        {
            std::cout << "\033[1;32mT\033[0m"; // 绿色字体 T，重置字体颜色
        }
        else // 如果命题为假
        {
            std::cout << "\033[1;31mF\033[0m"; // 红色字体 F，重置字体颜色
        }

        if (highlight) // 如果需要高亮显示
        {
            std::cout << "\033[0m"; // 重置所有属性
        }

        std::cout << std::endl; // 换行
    }

    // 构建范式表达式
    std::vector<std::string> clauses; // 定义一个向量用于存储子句
    for (size_t i = 0; i < truth_table.size(); ++i) // 遍历真值表的每一行
    {
        bool prop_value = results[i]; // 获取命题的真值
        if ((mode != 0 && prop_value) || (mode == 0 && !prop_value)) // 如果需要构建子句
        {
            // 构建子句
            std::string clause; // 定义一个字符串用于存储子句
            if (mode != 0) // 如果模式不为 0
            {
                // 主析取范式（PDNF）
                std::vector<std::string> literals; // 定义一个向量用于存储文字
                for (size_t j = 0; j < vars.size(); ++j) // 遍历命题变元
                {
                    if (truth_table[i][j]) // 如果真值为真
                    {
                        literals.push_back(vars[j]); // 将命题变元添加到文字向量中
                    }
                    else // 如果真值为假
                    {
                        literals.push_back("!" + vars[j]); // 将命题变元的否定添加到文字向量中
                    }
                }
                clause = "( " + join(" && ", literals) + " )"; // 构建子句
            }
            else // 如果模式为 0
            {
                // 主合取范式（PCNF）
                std::vector<std::string> literals; // 定义一个向量用于存储文字
                for (size_t j = 0; j < vars.size(); ++j) // 遍历命题变元
                {
                    if (truth_table[i][j]) // 如果真值为真
                    {
                        literals.push_back("!" + vars[j]); // 将命题变元的否定添加到文字向量中
                    }
                    else // 如果真值为假
                    {
                        literals.push_back(vars[j]); // 将命题变元添加到文字向量中
                    }
                }
                clause = "( " + join(" || ", literals) + " )"; // 构建子句
            }
            clauses.push_back(clause); // 将子句添加到子句向量中
        }
    }

    // 合并子句
    std::string normal_form_expr; // 定义一个字符串用于存储范式表达式
    if (clauses.empty()) // 如果子句向量为空
    {
        if (mode != 0) // 如果模式不为 0
        {
            normal_form_expr = "0"; // 命题恒假
        }
        else // 如果模式为 0
        {
            normal_form_expr = "1"; // 命题恒真
        }
    }
    else // 如果子句向量不为空
    {
        if (mode != 0) // 如果模式不为 0
        {
            // 主析取范式（PDNF）
            normal_form_expr = join(" || ", clauses); // 合并子句
        }
        else // 如果模式为 0
        {
            // 主合取范式（PCNF）
            normal_form_expr = join(" && ", clauses); // 合并子句
        }
    }

    std::cout << "范式表达式: " << normal_form_expr << std::endl; // 打印范式表达式

    // 清空变量
    variables.clear(); // 清空全局变量映射

    if (mode == 0) // 如果模式为 0
        return 0; // 返回 0
    else // 如果模式不为 0
        return 1; // 返回 1
}







int main()
{
#ifdef _WIN32
    system("chcp 65001 > nul");
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32
    bool flag = 0;
    string input_str;
    printf("Tiny_Pyhon 0.2 (tags/v0.2:hash, Sep. 13 2024, 19:50:41) [MSC v.1929 64 bit (AMD64)] on win32\nType \"help\", \"copyright\", \"credits\" or \"license\" for more information.\n");
    while (1)
    {
        clear_stack(temp_suffix_result, operators);
        if (!flag)
            cout << ">>> ";
        else
            flag = 0;
        getline(cin, input_str);
        if (input_str.empty())
        {
            flag = 1;
            cout << ">>> ";
            continue;
        }
        if (input_str == "exit")
            break;
        SYMBOL var;
        try
        {
            executer(&input_str, &var);
        }
        catch (const std::invalid_argument &e)
        {
            clear_stack(temp_suffix_result, operators);
            std::cerr << e.what() << std::endl;
        }
    }

    return 0;
}
