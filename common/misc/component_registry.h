#ifndef COMPONENT_REGISTRY_H
#define COMPONENT_REGISTRY_H

#include <string>
#include <map>
#include <functional>

template <typename BaseType, typename... Args>
class ComponentRegistry
{
public:
   typedef std::function<BaseType*(Args...)> CreatorFunc;

   static ComponentRegistry& get()
   {
      static ComponentRegistry instance;
      return instance;
   }

   void registerComponent(const std::string& name, CreatorFunc func)
   {
      m_creators[name] = func;
   }

   BaseType* create(const std::string& name, Args... args)
   {
      auto it = m_creators.find(name);
      if (it != m_creators.end()) {
         return it->second(args...);
      }
      return nullptr;
   }

   bool hasComponent(const std::string& name) const
   {
      return m_creators.find(name) != m_creators.end();
   }

private:
   ComponentRegistry() {}
   std::map<std::string, CreatorFunc> m_creators;
};

template <typename BaseType, typename... Args>
class ComponentRegistrar
{
public:
   ComponentRegistrar(const std::string& name, typename ComponentRegistry<BaseType, Args...>::CreatorFunc func)
   {
      ComponentRegistry<BaseType, Args...>::get().registerComponent(name, func);
   }
};

#endif // COMPONENT_REGISTRY_H
