//
//  rcv_proxy.h
//  rcv
//
//  Created by Jackie Ou on 2025/1/17.
//  Copyright © 2025 RingCentral. All rights reserved.
//

#include "proxy.hpp"
#include <iostream>

namespace base {
    namespace details {
        const static std::string TAG("ScopedTrace");
        ScopedTrace::ScopedTrace(const char* class_and_method_name)
            : class_and_method_name_(class_and_method_name) {
            std::cout << TAG << " +: " << class_and_method_name_ << std::endl;
        }
        ScopedTrace::~ScopedTrace() {
            std::cout << TAG << " -: " << class_and_method_name_ << std::endl;
        }
    }  // namespace details
}  // namespace base
