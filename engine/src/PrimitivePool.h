#pragma once
#include <vector>
#include <deque>

namespace imp
{
    template<typename T, typename Factory, typename FactoryArgs>
    class PrimitivePool
    {
        public:

        PrimitivePool() = default;

        T Acquire(const FactoryArgs& args)
        {
            if (m_Pool.size())
            {
                T item = m_Pool.back();
                m_Pool.pop_back();
                return item;
            }

            return Factory::Create(args);
        }

        void Release(const T& item)
        {
            m_Pool.push_back(item);
        }

        void Destroy(const FactoryArgs& args)
        {
            for (const auto& item : m_Pool)
                Factory::Destroy(item, args);
        }

        private:

        std::vector<T> m_Pool;
    };

    // Attaching a point puts this Primitive in a "Timeline"
    // where given the latest synced SubmitSync point we can compare whether
    // this primitive is also synced and therefore safe to use
    template<typename T>
    struct PrimitiveInTimeline
    {
        T primitive;
        uint64_t point;
    };

    template<typename T, typename Factory, typename FactoryArgs>
    class PrimitiveInTimelinePool
    {
        public:

        PrimitiveInTimelinePool() = default;

        T Acquire(const FactoryArgs& args, uint64_t lastSynced)
        {
            if (m_Pool.size() && m_Pool.front().point <= lastSynced)
            {
                PrimitiveInTimeline<T> item = m_Pool.front();
                m_Pool.pop_front();
                return item.primitive;
            }

            return Factory::Create(args);
        }

        void Release(const T& item, uint64_t point)
        {
            m_Pool.emplace_back(item, point);
        }

        void Destroy(const FactoryArgs& args)
        {
            for (const auto& item : m_Pool)
                Factory::Destroy(item.primitive, args);
        }

        private:

        std::deque<PrimitiveInTimeline<T>> m_Pool;
    };
}