#include <iostream>
#include <cmath>

// Dual number structure for automatic differentiation
struct Dual {
    double value;      // Function value
    double derivative; // Derivative value

    Dual(double v, double d) : value(v), derivative(d) {}

    // Overload addition
    Dual operator+(const Dual& other) const {
        return Dual(value + other.value, derivative + other.derivative);
    }

    // Overload subtraction
    Dual operator-(const Dual& other) const {
        return Dual(value - other.value, derivative - other.derivative);
    }

    // Overload multiplication
    Dual operator*(const Dual& other) const {
        return Dual(value * other.value, value * other.derivative + derivative * other.value);
    }

    // Overload division
    Dual operator/(const Dual& other) const {
        return Dual(value / other.value, 
                    (derivative * other.value - value * other.derivative) / (other.value * other.value));
    }

    // Overload unary minus
    Dual operator-() const {
        return Dual(-value, -derivative);
    }
};

// Exponential function
Dual exp(const Dual& x) {
    double exp_value = std::exp(x.value);
    return Dual(exp_value, exp_value * x.derivative);
}

// Logarithm function
Dual log(const Dual& x) {
    return Dual(std::log(x.value), x.derivative / x.value);
}

// Function to compute f(x) = x^2 + 2x + exp(x)
Dual compute_function(const Dual& x) {
    return x * x + Dual(2.0, 0.0) * x + exp(x);
}

int main() {
    // Initialize x = 1.0 with derivative 1.0 (i.e., d(x)/dx = 1)
    Dual x(1.0, 1.0);

    // Compute the function and its derivative
    Dual result = compute_function(x);
    
    std::cout << "Function value at x = " << x.value << " is " << result.value << std::endl;
    std::cout << "Derivative at x = " << x.value << " is " << result.derivative << std::endl;

    return 0;
}
