#include "Circle.hpp"

using namespace asge::math;

std::vector<Int2> asge::math::MidpointCirclePoints(Int2 inCenter, int inRadius)
{
    std::vector<Int2> outPoints;
    int x = inRadius, y = 0, err = 1 - inRadius;
    const int dx[4] = { 1, -1,  1, -1 };
    const int dy[4] = { 1, -1, -1,  1 };

    while ( x >= y )
    {
        for ( std::size_t ii = 0; ii < 4; ++ii )
        {
            outPoints.push_back( { inCenter.x() + dx[ii] * x, inCenter.y() + dy[ii] * y } );
            outPoints.push_back( { inCenter.x() + dx[ii] * y, inCenter.y() + dy[ii] * x } );
        }

        y++;
        err += ( err < 0 ) ? 2 * y + 1 : 2 * ( y - (--x) ) + 1;
    }

    return outPoints;
}