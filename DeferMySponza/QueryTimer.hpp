#pragma once

#include <vector>
#include <map>
#include <functional>

#include <tgl/tgl.h>

class QueryTimer
{
    struct Query
    {
        bool inUse;
        GLuint start, end;
    };

    int idCounter = 0;

    std::map<int, std::shared_ptr<QueryTimer::Query>> queries;
        
    std::function<void(GLuint64, GLuint64)> onQueryReady;

    std::pair<int, std::shared_ptr<QueryTimer::Query>> GetUnusedQuery();

public:
    QueryTimer(std::function<void(GLuint64, GLuint64)> onReadyfunc_);

    int Start();
    void End(int id_);

    void Check();

};