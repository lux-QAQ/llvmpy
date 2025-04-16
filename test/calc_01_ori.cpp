#include <iostream>
#include <stack>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <regex>
#include <cmath>
#include <functional>

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
优化原始表达式，方便转化为逆波兰表达式
*/
void Expression_optimization(string *str)
{

    std::regex pattern("\\*\\*"); // 正则表达式中需要转义的字符
    std::string replacement = "^";

    // 使用 std::regex_replace 进行替换
    *str = std::regex_replace(*str, pattern, replacement);
    /*在单独的负数，负号前面插入0*/
    for (int i = 0; i < str->size(); i++)
    {
        if (str->at(i) == '-' && (i == 0 || str->at(i - 1) == '('))
        {
            str->insert(i, "0");
        }
    }
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
            if (temp == "max" || temp == "min" || temp == "log" || temp == "sin" || temp == "cos")
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
            if (!operators.empty()) operators.pop(); // 移除开括号
            if (!operators.empty() && (operators.top() == "max" || operators.top() == "min" || operators.top() == "log" || operators.top() == "sin" || operators.top() == "cos"))
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
                    throw std::invalid_argument("无效的操作符位置");
                }
            }
            process_operator(current, operators, temp_suffix_result);
            lastWasOperatorOrOpenParenthesis = false;
            i++;
        }
        else
        {
            throw std::invalid_argument("无效的字符: " + std::string(1, current));
        }
    }

    while (!operators.empty())
    {
        temp_suffix_result.push(operators.top());
        operators.pop();
    }
}

void clear_stack(stack<string> &s, stack<string> &o)
{
    stack<string> empty;
    s.swap(empty);
    stack<string> empty2;
    o.swap(empty2);
}

// 定义函数类型
using OperatorFunc = std::function<double(double, double)>;
using FunctionFunc = std::function<double(double)>;

// 定义操作函数
double add(double left, double right) { return left + right; }
double subtract(double left, double right) { return left - right; }
double multiply(double left, double right) { return left * right; }
double divide(double left, double right) { return left / right; }
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
        { // Handle variable usage
            if (variables.find(current) != variables.end())
            {
                temp_resulet.push(std::to_string(variables[current]));
            }
            else if (functionMap.find(current) != functionMap.end())
            {
                if (temp_resulet.size() < 1)
                {
                    clear_stack(temp_suffix_result, operators);
                    throw std::invalid_argument("无效的表达式");
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
                    clear_stack(temp_suffix_result, operators);
                    throw std::invalid_argument("无效的表达式");
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
                clear_stack(temp_suffix_result, operators);
                throw std::invalid_argument("变量 " + current + "未定义.");
            }
        }
        else
        {
            if (temp_resulet.size() < 2)
            {
                clear_stack(temp_suffix_result, operators);
                throw std::invalid_argument("无效的表达式");
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
        clear_stack(temp_suffix_result, operators);
        throw std::invalid_argument("无效的表达式");
    }

    return std::stod(temp_resulet.top());
}

void executer(string *str, SYMBOL *var)
{
    Expression_optimization(str);

    size_t equal_pos = str->find('=');
    if (equal_pos != string::npos)
    {
        string var_name = str->substr(0, equal_pos);
        if (isdigit(var_name[0]))
        {
            clear_stack(temp_suffix_result, operators);
            throw std::invalid_argument("变量名不能以数字开头.");
        }
        string expression = str->substr(equal_pos + 1);
        lexer(&expression);
        double result = calculate(&expression, temp_suffix_result);
        variables[var_name] = result;
        std::cout << var_name << " = " << result << std::endl;
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

int main()
{
    bool flag = 0;
    string input_str;
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

            std::cerr << e.what() << std::endl;
        }
    }

    return 0;
}