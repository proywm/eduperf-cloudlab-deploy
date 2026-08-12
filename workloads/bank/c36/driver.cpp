#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <random>
#include <algorithm>

using uint = uint32_t;
using Scalar = double;

// ---- tiny faithful stubs (Eigen-like Vector3, Triangle) ----
struct Vector3 {
    Scalar x=0,y=0,z=0;
    Vector3()=default;
    Vector3(Scalar a,Scalar b,Scalar c):x(a),y(b),z(c){}
    Vector3 operator+(const Vector3&o)const{return {x+o.x,y+o.y,z+o.z};}
    Vector3 operator-(const Vector3&o)const{return {x-o.x,y-o.y,z-o.z};}
    friend Vector3 operator*(Scalar s,const Vector3&v){return {s*v.x,s*v.y,s*v.z};}
    void normalize(){Scalar n=std::sqrt(x*x+y*y+z*z); if(n>0){x/=n;y/=n;z/=n;}}
    bool operator==(const Vector3&o)const{return x==o.x&&y==o.y&&z==o.z;}
};
struct Triangle {
    uint a,b,c;
    Triangle(uint x,uint y,uint z):a(x),b(y),c(z){}
    bool operator==(const Triangle&o)const{return a==o.a&&b==o.b&&c==o.c;}
};
constexpr Scalar PiMul2 = 6.283185307179586;

static void getOrthogonalVectors(const Vector3& v, Vector3& x, Vector3& y){
    // faithful-enough: pick an axis not parallel, cross products
    Vector3 a = (std::abs(v.x) < 0.9) ? Vector3(1,0,0) : Vector3(0,1,0);
    x = Vector3(v.y*a.z - v.z*a.y, v.z*a.x - v.x*a.z, v.x*a.y - v.y*a.x);
    y = Vector3(v.y*x.z - v.z*x.y, v.z*x.x - v.x*x.z, v.x*x.y - v.y*x.x);
}

namespace v_before {
struct TriangleMesh {
    std::vector<Vector3> m_vertices;
    std::vector<Vector3> m_normals;
    std::vector<Triangle> m_triangles;
};
static void getAutoNormals(TriangleMesh& m, std::vector<Vector3>& normals){
    normals.assign(m.m_vertices.size(), Vector3(0,0,1)); // deterministic stub
}
TriangleMesh makeTube( const Vector3& a, const Vector3& b, Scalar outerRadius, Scalar innerRadius,
                       uint nFaces ) {
    TriangleMesh result;

    Vector3 ab = b - a;
    Vector3 xPlane, yPlane;
    getOrthogonalVectors( ab, xPlane, yPlane );
    xPlane.normalize();
    yPlane.normalize();
    Vector3 c = 0.5 * ( a + b );
    const Scalar thetaInc( PiMul2 / Scalar( nFaces ) );
    for ( uint i = 0; i < nFaces; ++i ) {
        const Scalar theta = i * thetaInc;
        result.m_vertices.push_back( a + outerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( c + outerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( b + outerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( a + innerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( c + innerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( b + innerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
    }
    for ( uint i = 0; i < nFaces; ++i ) {
        uint obl = 6 * i;
        uint obr = 6 * ( ( i + 1 ) % nFaces );
        uint oml = obl + 1;
        uint omr = obr + 1;
        uint otl = oml + 1;
        uint otr = omr + 1;
        uint ibl = 6 * i + 3;
        uint ibr = 6 * ( ( i + 1 ) % nFaces ) + 3;
        uint iml = ibl + 1;
        uint imr = ibr + 1;
        uint itl = iml + 1;
        uint itr = imr + 1;
        result.m_triangles.push_back( Triangle( obl, obr, oml ) );
        result.m_triangles.push_back( Triangle( obr, omr, oml ) );
        result.m_triangles.push_back( Triangle( oml, omr, otl ) );
        result.m_triangles.push_back( Triangle( omr, otr, otl ) );
        result.m_triangles.push_back( Triangle( ibr, ibl, iml ) );
        result.m_triangles.push_back( Triangle( ibr, iml, imr ) );
        result.m_triangles.push_back( Triangle( imr, iml, itl ) );
        result.m_triangles.push_back( Triangle( imr, itl, itr ) );
        result.m_triangles.push_back( Triangle( ibr, obr, ibl ) );
        result.m_triangles.push_back( Triangle( obl, ibl, obr ) );
        result.m_triangles.push_back( Triangle( otr, itr, itl ) );
        result.m_triangles.push_back( Triangle( itl, otl, otr ) );
    }
    getAutoNormals( result, result.m_normals );
    return result;
}
} // v_before

namespace v_after {
struct TriangleMesh {
    std::vector<Vector3> m_vertices;
    std::vector<Vector3> m_normals;
    std::vector<Triangle> m_triangles;
};
static void getAutoNormals(TriangleMesh& m, std::vector<Vector3>& normals){
    normals.assign(m.m_vertices.size(), Vector3(0,0,1));
}
TriangleMesh makeTube( const Vector3& a, const Vector3& b, Scalar outerRadius, Scalar innerRadius,
                       uint nFaces ) {
    TriangleMesh result;
    result.m_vertices.reserve(6*nFaces);
    result.m_normals.reserve(6*nFaces);
    result.m_triangles.reserve(12*nFaces);

    Vector3 ab = b - a;
    Vector3 xPlane, yPlane;
    getOrthogonalVectors( ab, xPlane, yPlane );
    xPlane.normalize();
    yPlane.normalize();
    Vector3 c = 0.5 * ( a + b );
    const Scalar thetaInc( PiMul2 / Scalar( nFaces ) );
    for ( uint i = 0; i < nFaces; ++i ) {
        const Scalar theta = i * thetaInc;
        result.m_vertices.push_back( a + outerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( c + outerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( b + outerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( a + innerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( c + innerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
        result.m_vertices.push_back( b + innerRadius * ( std::cos( theta ) * xPlane + std::sin( theta ) * yPlane ) );
    }
    for ( uint i = 0; i < nFaces; ++i ) {
        uint obl = 6 * i;
        uint obr = 6 * ( ( i + 1 ) % nFaces );
        uint oml = obl + 1;
        uint omr = obr + 1;
        uint otl = oml + 1;
        uint otr = omr + 1;
        uint ibl = 6 * i + 3;
        uint ibr = 6 * ( ( i + 1 ) % nFaces ) + 3;
        uint iml = ibl + 1;
        uint imr = ibr + 1;
        uint itl = iml + 1;
        uint itr = imr + 1;
        result.m_triangles.push_back( Triangle( obl, obr, oml ) );
        result.m_triangles.push_back( Triangle( obr, omr, oml ) );
        result.m_triangles.push_back( Triangle( oml, omr, otl ) );
        result.m_triangles.push_back( Triangle( omr, otr, otl ) );
        result.m_triangles.push_back( Triangle( ibr, ibl, iml ) );
        result.m_triangles.push_back( Triangle( ibr, iml, imr ) );
        result.m_triangles.push_back( Triangle( imr, iml, itl ) );
        result.m_triangles.push_back( Triangle( imr, itl, itr ) );
        result.m_triangles.push_back( Triangle( ibr, obr, ibl ) );
        result.m_triangles.push_back( Triangle( obl, ibl, obr ) );
        result.m_triangles.push_back( Triangle( otr, itr, itl ) );
        result.m_triangles.push_back( Triangle( itl, otl, otr ) );
    }
    getAutoNormals( result, result.m_normals );
    return result;
}
} // v_after

template<class M1, class M2>
bool meshEq(const M1& x, const M2& y){
    if(x.m_vertices.size()!=y.m_vertices.size()) return false;
    if(x.m_normals.size()!=y.m_normals.size()) return false;
    if(x.m_triangles.size()!=y.m_triangles.size()) return false;
    for(size_t i=0;i<x.m_vertices.size();++i) if(!(x.m_vertices[i]==y.m_vertices[i])) return false;
    for(size_t i=0;i<x.m_normals.size();++i) if(!(x.m_normals[i]==y.m_normals[i])) return false;
    for(size_t i=0;i<x.m_triangles.size();++i) if(!(x.m_triangles[i]==y.m_triangles[i])) return false;
    return true;
}

int main(){
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> ud(-5,5);
    std::uniform_real_distribution<double> rd(0.01,10.0);

    // differential correctness over diverse/boundary inputs
    std::vector<uint> faceBattery = {1,2,3,4,5,6,7,8,12,16,31,64,100,255,256,257,1000,4096};
    bool ok = true;
    std::string diverge;
    for(int t=0;t<3000 && ok;++t){
        Vector3 A(ud(rng),ud(rng),ud(rng));
        Vector3 B(ud(rng),ud(rng),ud(rng));
        double r1=rd(rng), r2=rd(rng);
        double outer=std::max(r1,r2), inner=std::min(r1,r2);
        uint nf = faceBattery[t % faceBattery.size()];
        auto mb = v_before::makeTube(A,B,outer,inner,nf);
        auto ma = v_after::makeTube(A,B,outer,inner,nf);
        if(!meshEq(mb,ma)){ ok=false; diverge = "A=("+std::to_string(A.x)+","+std::to_string(A.y)+","+std::to_string(A.z)+") nf="+std::to_string(nf); }
    }
    // explicit boundary: nFaces = 1
    {
        auto mb = v_before::makeTube(Vector3(0,0,0),Vector3(1,1,1),2.0,1.0,1);
        auto ma = v_after::makeTube(Vector3(0,0,0),Vector3(1,1,1),2.0,1.0,1);
        if(!meshEq(mb,ma)){ ok=false; diverge="nf=1 boundary"; }
    }
    printf("EQUIVALENT=%d %s\n", ok?1:0, diverge.c_str());

    // interleaved timing
    const int reps = 4000;
    uint nf = 2000;
    Vector3 A(0.3,-1.2,4.5), B(2.1,3.3,-0.7);
    std::vector<double> tb, ta;
    volatile size_t sink=0;
    for(int r=0;r<reps;++r){
        if(r&1){
            auto s1=std::chrono::high_resolution_clock::now();
            auto mb=v_before::makeTube(A,B,3.0,1.0,nf);
            auto e1=std::chrono::high_resolution_clock::now();
            sink+=mb.m_triangles.size();
            auto s2=std::chrono::high_resolution_clock::now();
            auto ma=v_after::makeTube(A,B,3.0,1.0,nf);
            auto e2=std::chrono::high_resolution_clock::now();
            sink+=ma.m_triangles.size();
            tb.push_back(std::chrono::duration<double,std::nano>(e1-s1).count());
            ta.push_back(std::chrono::duration<double,std::nano>(e2-s2).count());
        } else {
            auto s2=std::chrono::high_resolution_clock::now();
            auto ma=v_after::makeTube(A,B,3.0,1.0,nf);
            auto e2=std::chrono::high_resolution_clock::now();
            sink+=ma.m_triangles.size();
            auto s1=std::chrono::high_resolution_clock::now();
            auto mb=v_before::makeTube(A,B,3.0,1.0,nf);
            auto e1=std::chrono::high_resolution_clock::now();
            sink+=mb.m_triangles.size();
            tb.push_back(std::chrono::duration<double,std::nano>(e1-s1).count());
            ta.push_back(std::chrono::duration<double,std::nano>(e2-s2).count());
        }
    }
    std::sort(tb.begin(),tb.end());
    std::sort(ta.begin(),ta.end());
    double mb_ns=tb[tb.size()/2], ma_ns=ta[ta.size()/2];
    printf("before_ns=%.1f after_ns=%.1f speedup=%.3f sink=%zu\n", mb_ns, ma_ns, mb_ns/ma_ns, (size_t)sink);
    return 0;
}
