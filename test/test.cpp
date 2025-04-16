#include <iostream>
#include <stack>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <cmath>
#include <functional>

//#define DEBUG

using namespace std;

stack<string> operators;
stack<string> temp_suffix_result;
unordered_map<string, double> variables; // Global container for variables

typedef struct symbol
{
    string name;
    string data;
    int typ;
    int pri;
    int pos;
    double val;
} SYMBOL;
void clear_stack(stack<string> &s, stack<string> &o)
{
    stack<string> empty;
    s.swap(empty);
    stack<string> empty2;
    o.swap(empty2);
}
/*
移除字符串中的空格
*/
string remove_spaces(string *str)
{
    str->erase(std::remove(str->begin(), str->end(), ' '), str->end());
    return *str;
}

/*
优先级map
*/
std::unordered_map<char, int> precedence = {
    {'+', 1},
    {'-', 1},
    {'*', 2},
    {'/', 2},
    {'^', 3}};

/*
函数名集合
*/
std::unordered_set<std::string> functions = {"max", "min", "log", "sin", "cos"};

/*
处理操作符
*/
void process_operator(char op, std::stack<std::string> &operators, std::stack<std::string> &temp_suffix_result)
{
    while (!operators.empty() && operators.top() != "(" &&
           precedence[operators.top()[0]] >= precedence[op])
    {
        temp_suffix_result.push(operators.top());
        operators.pop();
    }
    operators.push(std::string(1, op));
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
    string level_output;
    string color_code;

    // 根据提示等级设置颜色和提示类型
    if (Message_level == "error")
    {
        level_output = "\033[1;31merror\033[0m"; // 红色
        color_code = "\033[1;31m";               // 红色
    }
    else if (Message_level == "warning")
    {
        level_output = "\033[1;33mwarning\033[0m"; // 黄色
        color_code = "\033[1;33m";                 // 黄色
    }
    else if (Message_level == "info")
    {
        level_output = "\033[1;36minfo\033[0m"; // 天蓝色 (青色)
        color_code = "\033[1;36m";              // 天蓝色 (青色)
    }

    // 输出提示类型
    cout << level_output << ": at position " << pos << endl;

    // 输出带颜色的原始字符串
    cout << "        " << oriStr.substr(0, pos);              // 错误前的部分，默认颜色
    cout << color_code << oriStr.substr(pos, 1) << "\033[0m"; // 错误字符，带颜色
    cout << oriStr.substr(pos + 1) << endl;                   // 错误后面的部分，默认颜色

    // 输出指向错误字符的波浪线，长度和错误字符保持一致
    cout << "        " << string(pos, ' ') << color_code << "^";
    cout << string(oriStr.length() - pos - 1, '~') << "\033[0m" << endl;

    // 输出建议替代方案
    if (!suggestion.empty())
    {
        cout << "note: suggested alternative: '" << suggestion << "'" << endl;
    }

    // 如果是 error，终止程序
    if (Message_level == "error")
    {
        clear_stack(temp_suffix_result, operators);
        throw std::invalid_argument("本轮循环因异常而终止"); // 终止当前循环
    }
}

// 定义函数类型
using OperatorFunc = std::function<double(double, double)>;
using FunctionFunc = std::function<double(double)>;

// 定义操作函数
double add(double left, double right) { return left + right; }
double subtract(double left, double right) { return left - right; }
double multiply(double left, double right) { return left * right; }
double divide(double left, double right) {if (right == 0)
{
    clear_stack(temp_suffix_result, operators);
    throw std::invalid_argument("\033[1;31m除数不能为0\033[0m");
}
 return left / right; }
double power(double left, double right) { return std::pow(left, right); }
double max_func(double left, double right) { return std::max(left, right); }
double min_func(double left, double right) { return std::min(left, right); }
double log_func(double left, double right) { return std::log(right) / std::log(left); }
double sin_func(double x) { return std::sin(x); }
double cos_func(double x) { return std::cos(x); }

// 操作符映射表
std::unordered_map<std::string, OperatorFunc> operatorMap = {
    {"+", add},
    {"-", subtract},
    {"*", multiply},
    {"/", divide},
    {"^", power},
    {"max", max_func},
    {"min", min_func},
    {"log", log_func}};

// 函数映射表
std::unordered_map<std::string, FunctionFunc> functionMap = {
    {"sin", sin_func},
    {"cos", cos_func}};


// 替换表
std::unordered_map<std::string, std::string> replacementMap = {
    {"\\*\\*", "^"}
};

/*
优化原始表达式，方便转化为逆波兰表达式
*/
void Expression_optimization(string *str)
{
    // 遍历字符串，查找并替换大写的函数名为小写
    for (size_t i = 0; i < str->size(); ++i)
    {
        if (isalpha(str->at(i)))
        {
            size_t start = i;
            while (i < str->size() && isalpha(str->at(i)))
            {
                ++i;
            }
            string token = str->substr(start, i - start);
            string lower_token = token;
            transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
            if (functions.find(lower_token) != functions.end())
            {
                // 只有在函数名包含大写字母时才调用 Hint
                if (token != lower_token)
                {
                    // 提示用户并指明位置
                    Hint(*str, "info", start, lower_token);
                    str->replace(start, token.size(), lower_token);
                }
            }
        }
    }

    // 使用替换表进行替换
    for (const auto &pair : replacementMap)
    {
        std::regex pattern(pair.first);
        std::string replacement = pair.second;
        std::smatch match;
        std::string temp_str = *str;
        while (std::regex_search(temp_str, match, pattern))
        {
            size_t pos = match.position();
            Hint(*str, "info", pos, replacement);
            *str = std::regex_replace(*str, pattern, replacement);
            temp_str = *str;
        }
    }

    // 在单独的负数，负号前面插入0
    for (int i = 0; i < str->size(); i++)
    {
        if (str->at(i) == '-' && (i == 0 || str->at(i - 1) == '('))
        {
            str->insert(i, "0");
        }
    }

    #ifdef DEBUG
    cout << "Expression_optimization: " << *str << endl;
    #endif
}

void lexer(string *str)
{
    remove_spaces(str);
    int i = 0;
    bool lastWasOperatorOrOpenParenthesis = true; // 用于跟踪上一个字符是否为操作符或 '('

    while (i < str->size())
    {
        char current = str->at(i);

        if (isdigit(current))
        { // 处理连续的数字
            std::string temp;
            while (i < str->size() && (isdigit(str->at(i)) || str->at(i) == '.'))
            {
                temp.push_back(str->at(i));
                i++;
            }
            temp_suffix_result.push(temp);
            lastWasOperatorOrOpenParenthesis = false;
        }
        else if (isalpha(current))
        { // 处理变量名或函数名
            std::string temp;
            while (i < str->size() && isalpha(str->at(i)))
            {
                temp.push_back(str->at(i));
                i++;
            }
            if (functions.find(temp) != functions.end())
            {
                operators.push(temp);
            }
            else
            {
                temp_suffix_result.push(temp);
            }
            lastWasOperatorOrOpenParenthesis = false;
        }
        else if (current == '(')
        {
            operators.push("(");
            lastWasOperatorOrOpenParenthesis = true;
            i++;
        }
        else if (current == ')')
        {
            while (!operators.empty() && operators.top() != "(")
            {
                temp_suffix_result.push(operators.top());
                operators.pop();
            }
            if (!operators.empty())
                operators.pop(); // 移除开括号
            if (!operators.empty() && functions.find(operators.top()) != functions.end())
            {
                temp_suffix_result.push(operators.top());
                operators.pop();
            }
            lastWasOperatorOrOpenParenthesis = false;
            i++;
        }
        else if (current == ',')
        { // 处理函数参数分隔符
            while (!operators.empty() && operators.top() != "(")
            {
                temp_suffix_result.push(operators.top());
                operators.pop();
            }
            i++;
        }
        else if (precedence.find(current) != precedence.end())
        { // 处理操作符
            if (lastWasOperatorOrOpenParenthesis)
            {
                if (current == '-')
                {
                    temp_suffix_result.push("0"); // 在负号前加一个零
                }
                else
                {
                    clear_stack(temp_suffix_result, operators);
                    Hint(*str, "error", i, "错误的操作符");
                }
            }
            process_operator(current, operators, temp_suffix_result);
            lastWasOperatorOrOpenParenthesis = false;
            i++;
        }
        else
        {
            clear_stack(temp_suffix_result, operators);
            Hint(*str, "error", i, "未知的字符");
        }
    }

    while (!operators.empty())
    {
        temp_suffix_result.push(operators.top());
        operators.pop();
    }
}

double Binary_Computing_Executor(double left, double right, const std::string op)
{
    auto it = operatorMap.find(op);
    if (it != operatorMap.end())
    {
        return it->second(left, right);
    }
    clear_stack(temp_suffix_result, operators);
    throw std::invalid_argument("未知的运算符: " + op);
}

double Unary_Computing_Executor(double value, const std::string func)
{
    auto it = functionMap.find(func);
    if (it != functionMap.end())
    {
        return it->second(value);
    }
    clear_stack(temp_suffix_result, operators);
    throw std::invalid_argument("未知的函数: " + func);
}

double calculate(string *str, stack<string> temp_suffix_result)
{
    std::stack<string> temp_resulet, temp_suffix;

    // 将 temp_suffix_result 反转到 temp_suffix
    while (!temp_suffix_result.empty())
    {
        temp_suffix.push(temp_suffix_result.top());
        temp_suffix_result.pop();
    }

    while (!temp_suffix.empty())
    {
        std::string current = temp_suffix.top();
        temp_suffix.pop();

        if (isdigit(current[0]))
        {
            temp_resulet.push(current);
        }
        else if (isalpha(current[0]))
        { // 处理变量使用
            if (variables.find(current) != variables.end())
            {
                temp_resulet.push(std::to_string(variables[current]));
            }
            else if (functionMap.find(current) != functionMap.end())
            {
                if (temp_resulet.size() < 1)
                {
                    Hint(*str, "error", str->find(current), "无效的表达式: 函数缺少参数");
                    clear_stack(temp_suffix_result, operators);
                    return 0; // 终止处理
                }
                double value = std::stod(temp_resulet.top());
                temp_resulet.pop();
                double result = Unary_Computing_Executor(value, current);
                temp_resulet.push(std::to_string(result));
            }
            else if (operatorMap.find(current) != operatorMap.end())
            {
                if (temp_resulet.size() < 2)
                {
                    Hint(*str, "error", str->find(current), "无效的表达式: 操作符缺少参数");
                    clear_stack(temp_suffix_result, operators);
                    return 0; // 终止处理
                }
                double right = std::stod(temp_resulet.top());
                temp_resulet.pop();
                double left = std::stod(temp_resulet.top());
                temp_resulet.pop();
                double result = Binary_Computing_Executor(left, right, current);
                temp_resulet.push(std::to_string(result));
            }
            else
            {
                Hint(*str, "error", str->find(current), "变量 '" + current + "' 未定义");
                clear_stack(temp_suffix_result, operators);
                return 0; // 终止处理
            }
        }
        else
        {
            if (temp_resulet.size() < 2)
            {
                Hint(*str, "error", str->find(current), "无效的表达式: 缺少参数");
                clear_stack(temp_suffix_result, operators);
                return 0; // 终止处理
            }
            double right = std::stod(temp_resulet.top());
            temp_resulet.pop();
            double left = std::stod(temp_resulet.top());
            temp_resulet.pop();
            double result = Binary_Computing_Executor(left, right, current);
            temp_resulet.push(std::to_string(result));
        }
    }

    if (temp_resulet.size() != 1)
    {
        Hint(*str, "error", str->length(), "无效的表达式: 多余的操作数");
        clear_stack(temp_suffix_result, operators);
        return 0; // 终止处理
    }

    return std::stod(temp_resulet.top());
}

void create_variable(string var_name, string expression)
{
    if (isdigit(var_name[0]))
    {
        clear_stack(temp_suffix_result, operators);
        //throw std::invalid_argument("变量名不能以数字开头.");
    }

    if (functions.find(var_name) != functions.end())
    {
        clear_stack(temp_suffix_result, operators);
        Hint(var_name, "error", 0, "变量名不能与函数名重名");
        //throw std::invalid_argument("变量名不能与函数名重名.");
    }

    lexer(&expression);
    double result = calculate(&expression, temp_suffix_result);
    variables[var_name] = result;
    std::cout << var_name << " = " << result << std::endl;
}

void executer(string *str, SYMBOL *var)
{
    Expression_optimization(str);

    size_t equal_pos = str->find('=');
    if (equal_pos != string::npos)
    {
        string var_name = str->substr(0, equal_pos);
        string expression = str->substr(equal_pos + 1);
        create_variable(var_name, expression);
    }
    else
    {
        lexer(str);
        double result = calculate(str, temp_suffix_result);
        std::cout << result << std::endl;
    }

    std::stack<string> empty;
    temp_suffix_result.swap(empty);
    std::stack<string> empty2;
    operators.swap(empty2);
}
#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32
int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32
    bool flag = 0;
    string input_str;
    printf("Tiny_Pyhon 0.2 (tags/v0.2:hash, Sep. 13 2024, 19:50:41) [MSC v.1929 64 bit (AMD64)] on win32\nType \"help\", \"copyright\", \"credits\" or \"license\" for more information.\n");
    while (1)
    {
        std::string testStr = "max（1，2）";
for (const auto& pair : replacementMap) {
    size_t pos = 0;
    while ((pos = testStr.find(pair.first, pos)) != std::string::npos) {
        testStr.replace(pos, pair.first.length(), pair.second);
        pos += pair.second.length();
    }
}
std::cout << testStr << std::endl;  // 应该输出 max(1,2)
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