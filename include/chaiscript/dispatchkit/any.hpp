// This file is distributed under the BSD License.
// See "license.txt" for details.
// Copyright 2009-2012, Jonathan Turner (jonathan@emptycrate.com)
// and Jason Turner (jason@emptycrate.com)
// http://www.chaiscript.com

#ifndef CHAISCRIPT_ANY_HPP_
#define CHAISCRIPT_ANY_HPP_

#include <utility>
#include <iostream>
#include <atomic>
#include <typeinfo>
#include <cxxabi.h>
#include <vector>
#include <type_traits>

namespace chaiscript {
  namespace detail {
    
    // Global tracing for Any allocations
    static std::atomic<int> g_any_construct_count{0};
    static std::atomic<int> g_any_destruct_count{0};
    static std::atomic<int> g_data_impl_construct_count{0};
    static std::atomic<int> g_data_impl_destruct_count{0};
    static std::atomic<int> g_vector_float_any_count{0};
    
    inline std::string demangle(const char* name) {
      int status = -1;
      char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
      std::string result = (status == 0 && demangled) ? demangled : name;
      free(demangled);
      return result;
    }
    
    template<typename T>
    inline bool is_vector_float() {
      return std::is_same<typename std::decay<T>::type, std::vector<float>>::value;
    }
    
    namespace exception
    {
      /// \brief Thrown in the event that an Any cannot be cast to the desired type
      ///
      /// It is used internally during function dispatch.
      ///
      /// \sa chaiscript::detail::Any
      class bad_any_cast : public std::bad_cast
      {
        public:
          bad_any_cast() = default;

          bad_any_cast(const bad_any_cast &) = default;

          ~bad_any_cast() noexcept override = default;

          /// \brief Description of what error occurred
          const char * what() const noexcept override
          {
            return m_what.c_str();
          }

        private:
          std::string m_what = "bad any cast";
      };
    }
  

    class Any {
      private:
        struct Data
        {
          explicit Data(const std::type_info &t_type) 
            : m_type(t_type)
          {
          }

          Data &operator=(const Data &) = delete;

          virtual ~Data() = default;

          virtual void *data() = 0;

          const std::type_info &type() const
          {
            return m_type;
          }

          virtual std::unique_ptr<Data> clone() const = 0;
          const std::type_info &m_type;
        };

        template<typename T>
          struct Data_Impl : Data
          {
            explicit Data_Impl(T t_type)
              : Data(typeid(T)),
                m_data(std::move(t_type))
            {
              // int count = ++g_data_impl_construct_count;
              // if (count % 500 == 0) {
              //   std::string type_name = demangle(typeid(T).name());
              //   std::cout << "[ANY TRACE] Data_Impl<" << type_name << "> CONSTRUCT #" << count 
              //             << " | Active=" << (g_data_impl_construct_count - g_data_impl_destruct_count)
              //             << " | sizeof(T)=" << sizeof(T) << std::endl;
              // }
            }
            
            ~Data_Impl() override {
              // int count = ++g_data_impl_destruct_count;
              // if (count % 500 == 0) {
              //   std::string type_name = demangle(typeid(T).name());
              //   std::cout << "[ANY TRACE] Data_Impl<" << type_name << "> DESTRUCT #" << count 
              //             << " | Active=" << (g_data_impl_construct_count - g_data_impl_destruct_count)
              //             << std::endl;
              // }
            }

            void *data() override
            {
              return &m_data;
            }

            std::unique_ptr<Data> clone() const override
            {
              // static std::atomic<int> clone_count{0};
              // int count = ++clone_count;
              
              // if (is_vector_float<T>()) {
              //   if (count % 100 == 0) {
              //     const auto& vec = *reinterpret_cast<const std::vector<float>*>(&m_data);
              //     std::cout << "[ANY CLONE] vector<float> clone #" << count 
              //               << " | size=" << vec.size()
              //               << " | capacity=" << vec.capacity() << std::endl;
              //   }
              // }
              
              return std::unique_ptr<Data>(new Data_Impl<T>(m_data));
            }

            Data_Impl &operator=(const Data_Impl&) = delete;

            T m_data;
          };

        std::unique_ptr<Data> m_data;

      public:
        // construct/copy/destruct
        Any() = default;
        
        Any(Any &&t_any) noexcept {
          m_data = std::move(t_any.m_data);
          // int count = ++g_any_construct_count;
          // if (count % 500 == 0) {
          //   std::cout << "[ANY TRACE] Any MOVE-CONSTRUCT #" << count 
          //             << " | Active=" << (g_any_construct_count - g_any_destruct_count) << std::endl;
          // }
        }
        
        Any &operator=(Any &&t_any) = default;

        Any(const Any &t_any) 
        { 
          // int count = ++g_any_construct_count;
          // if (count % 500 == 0) {
          //   std::string type_name = t_any.empty() ? "void" : demangle(t_any.type().name());
          //   std::cout << "[ANY TRACE] Any COPY-CONSTRUCT from <" << type_name << "> #" << count 
          //             << " | Active=" << (g_any_construct_count - g_any_destruct_count) << std::endl;
          // }
          
          if (!t_any.empty())
          {
            m_data = t_any.m_data->clone(); 
          } else {
            m_data.reset();
          }
        }


        template<typename ValueType,
          typename = typename std::enable_if<!std::is_same<Any, typename std::decay<ValueType>::type>::value>::type>
        explicit Any(ValueType &&t_value)
          : m_data(std::unique_ptr<Data>(new Data_Impl<typename std::decay<ValueType>::type>(std::forward<ValueType>(t_value))))
        {
          // int count = ++g_any_construct_count;
          
          // // Special tracking for vector<float>
          // if (is_vector_float<ValueType>()) {
          //   int vf_count = ++g_vector_float_any_count;
          //   if (vf_count % 100 == 0) {
          //     const auto& vec = *reinterpret_cast<const std::vector<float>*>(&t_value);
          //     std::cout << "[ANY LEAK] vector<float> wrapped in Any #" << vf_count 
          //               << " | size=" << vec.size()
          //               << " | capacity=" << vec.capacity()
          //               << " | bytes=" << (vec.capacity() * sizeof(float))
          //               << " | Active_Any=" << (g_any_construct_count - g_any_destruct_count) << std::endl;
          //   }
          // }
          
          // if (count % 500 == 0) {
          //   std::string type_name = demangle(typeid(typename std::decay<ValueType>::type).name());
          //   std::cout << "[ANY TRACE] Any VALUE-CONSTRUCT <" << type_name << "> #" << count 
          //             << " | Active=" << (g_any_construct_count - g_any_destruct_count)
          //             << " | sizeof=" << sizeof(typename std::decay<ValueType>::type) << std::endl;
          // }
        }
        
        ~Any() {
          // int count = ++g_any_destruct_count;
          // if (count % 500 == 0) {
          //   std::string type_name = empty() ? "void" : demangle(type().name());
          //   std::cout << "[ANY TRACE] Any DESTRUCT <" << type_name << "> #" << count 
          //             << " | Active=" << (g_any_construct_count - g_any_destruct_count) << std::endl;
          // }
        }


        Any & operator=(const Any &t_any)
        {
          Any copy(t_any);
          swap(copy);
          return *this; 
        }

        template<typename ToType>
          ToType &cast() const
          {
            if (m_data && typeid(ToType) == m_data->type())
            {
              return *static_cast<ToType *>(m_data->data());
            } else {
              throw chaiscript::detail::exception::bad_any_cast();
            }
          }


        // modifiers
        Any & swap(Any &t_other)
        {
          std::swap(t_other.m_data, m_data);
          return *this;
        }

        // queries
        bool empty() const
        {
          return !bool(m_data);
        }

        const std::type_info & type() const
        {
          if (m_data) {
            return m_data->type();
          } else {
            return typeid(void);
          }
        }
    };

  }
}

#endif


