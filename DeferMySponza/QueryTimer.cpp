#include <memory>

#include "QueryTimer.hpp"



QueryTimer::QueryTimer(std::function<void(GLuint64, GLuint64)> onQueryReadyFunc_) :
onQueryReady(onQueryReadyFunc_)
{

}

std::pair<int, std::shared_ptr<QueryTimer::Query>> QueryTimer::GetUnusedQuery()
{

    for (auto it = queries.begin(); it != queries.end(); ++it)
    {
        if (!it->second->inUse)
        {
            return std::pair<int, std::shared_ptr<QueryTimer::Query>>(it->first, it->second);
        }
    }

    GLuint ids[2];
    glGenQueries(2, ids);

    int id = idCounter++;
    std::shared_ptr<QueryTimer::Query> query = std::make_shared<QueryTimer::Query>();
    query->start = ids[0];
    query->end = ids[1];
    queries[id] = query;
    return std::pair<int, std::shared_ptr<QueryTimer::Query>>(id, query);
}

int QueryTimer::Start()
{

    std::pair<int, std::shared_ptr<QueryTimer::Query>> pair = GetUnusedQuery();
    pair.second->inUse = true;

    glQueryCounter(pair.second->start, GL_TIMESTAMP);

    return pair.first;
}

void QueryTimer::End(int id_)
{
    glQueryCounter(queries[id_]->end, GL_TIMESTAMP);
}

void QueryTimer::Check()
{
    //printf("size: %u\n", queries.size());

    for (auto it = queries.begin(); it != queries.end(); ++it)
    {
        // if this timer object is actually inuse then query it
        if (it->second->inUse)
        {
            GLuint firstFinished = GL_FALSE, secondFinished = GL_FALSE;
            glGetQueryObjectuiv(it->second->start, GL_QUERY_RESULT_AVAILABLE, &firstFinished);
            glGetQueryObjectuiv(it->second->end, GL_QUERY_RESULT_AVAILABLE, &secondFinished);

            //printf("if: %u %u\n", firstFinished, secondFinished);

            if (firstFinished && secondFinished)
            {
                GLuint64 firstResult = 0;
                glGetQueryObjectui64v(it->second->start, GL_QUERY_RESULT, &firstResult);
                GLuint64 secondResult = 0;
                glGetQueryObjectui64v(it->second->end, GL_QUERY_RESULT, &secondResult);

                onQueryReady(firstResult, secondResult);

                it->second->inUse = false;
            }
        }
    }

}