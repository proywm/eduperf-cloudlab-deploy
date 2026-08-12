#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <chrono>
#include <algorithm>

// ---------- Tiny faithful stubs for the Uintah geometry types ----------
struct Vector {
  double x_,y_,z_;
  Vector():x_(0),y_(0),z_(0){}
  Vector(double a,double b,double c):x_(a),y_(b),z_(c){}
  double x() const {return x_;} double y() const {return y_;} double z() const {return z_;}
  double length() const {return std::sqrt(x_*x_+y_*y_+z_*z_);}
  double maxComponent() const {return std::max(x_,std::max(y_,z_));}
  Vector operator-(const Vector&o) const {return Vector(x_-o.x_,y_-o.y_,z_-o.z_);}
};
static inline double Dot(const Vector&a,const Vector&b){return a.x_*b.x_+a.y_*b.y_+a.z_*b.z_;}
static inline Vector Abs(const Vector&a){return Vector(std::fabs(a.x_),std::fabs(a.y_),std::fabs(a.z_));}

struct Point {
  double x_,y_,z_;
  Point():x_(0),y_(0),z_(0){}
  Point(double a,double b,double c):x_(a),y_(b),z_(c){}
  double x() const {return x_;} double y() const {return y_;} double z() const {return z_;}
  void x(double v){x_=v;} void y(double v){y_=v;} void z(double v){z_=v;}
  Vector asVector() const {return Vector(x_,y_,z_);}
  bool operator==(const Point&o) const {return x_==o.x_&&y_==o.y_&&z_==o.z_;}
  Point operator-(const Vector&v) const {return Point(x_-v.x_,y_-v.y_,z_-v.z_);}
  Point& operator-=(const Vector&v){x_-=v.x_;y_-=v.y_;z_-=v.z_;return *this;}
};
static inline Point Min(const Point&a,const Point&b){return Point(std::min(a.x_,b.x_),std::min(a.y_,b.y_),std::min(a.z_,b.z_));}
static inline Point Max(const Point&a,const Point&b){return Point(std::max(a.x_,b.x_),std::max(a.y_,b.y_),std::max(a.z_,b.z_));}

struct IntVector {
  int x_,y_,z_;
  IntVector():x_(0),y_(0),z_(0){}
  IntVector(int a,int b,int c):x_(a),y_(b),z_(c){}
  int x() const {return x_;} int y() const {return y_;} int z() const {return z_;}
};

struct Box {
  Point lo_,hi_;
  Box(){}
  Box(const Point&a,const Point&b):lo_(a),hi_(b){}
  Point lower() const {return lo_;}
  Point upper() const {return hi_;}
};

// A real triangle plane with a ray/plane intersection, so inside() does real work.
struct Plane {
  double a_,b_,c_,d_; // a*x+b*y+c*z + d = 0
  Vector n_;
  Plane():a_(0),b_(0),c_(1),d_(0),n_(0,0,1){}
  Plane(const Point&p0,const Point&p1,const Point&p2){
    Vector u(p1.x_-p0.x_,p1.y_-p0.y_,p1.z_-p0.z_);
    Vector v(p2.x_-p0.x_,p2.y_-p0.y_,p2.z_-p0.z_);
    n_ = Vector(u.y_*v.z_-u.z_*v.y_, u.z_*v.x_-u.x_*v.z_, u.x_*v.y_-u.y_*v.x_);
    a_=n_.x_; b_=n_.y_; c_=n_.z_;
    d_ = -(a_*p0.x_+b_*p0.y_+c_*p0.z_);
  }
  Vector normal() const {return n_;}
  // Intersect ray from origin p along direction dir; returns 1 if hits, fills hit.
  int Intersect(const Point&p,const Vector&dir,Point&hit) const {
    double denom = a_*dir.x_+b_*dir.y_+c_*dir.z_;
    if (std::fabs(denom) < 1e-300) return 0;
    double t = -(a_*p.x_+b_*p.y_+c_*p.z_+d_)/denom;
    hit = Point(p.x_+t*dir.x_, p.y_+t*dir.y_, p.z_+t*dir.z_);
    return 1;
  }
};

// ---------- The class, once per namespace, with inside() VERBATIM ----------
#define DEFINE_TRI(NS) \
namespace NS { \
class TriGeometryPiece { \
public: \
  std::vector<Point> d_points; \
  std::vector<IntVector> d_tri; \
  std::vector<Plane> d_planes; \
  std::vector<Box> d_boxes; \
  Box d_box; \
  bool inside(const Point &p) const; \
  void insideTriangle(const Point& q,int num,int& NCS,int& NES) const; \
}; \
void TriGeometryPiece::insideTriangle(const Point& q,int num,int& NCS,int& NES) const { \
  Vector plane_normal = d_planes[num].normal(); \
  Vector plane_normal_abs = Abs(plane_normal); \
  double largest = plane_normal_abs.maxComponent(); \
  int dominant_coord=3; \
  if (largest == plane_normal_abs.x()) dominant_coord = 1; \
  else if (largest == plane_normal_abs.y()) dominant_coord = 2; \
  else if (largest == plane_normal_abs.z()) dominant_coord = 3; \
  Point p[3]; \
  p[0] = d_points[d_tri[num].x()]; \
  p[1] = d_points[d_tri[num].y()]; \
  p[2] = d_points[d_tri[num].z()]; \
  Point trans_pt(0.,0.,0.), trans_vt[3]; \
  trans_vt[0] = Point(0.,0.,0.); trans_vt[1] = Point(0.,0.,0.); trans_vt[2] = Point(0.,0.,0.); \
  if (dominant_coord == 1) { trans_pt.x(q.y()); trans_pt.y(q.z()); for (int i=0;i<3;i++){ trans_vt[i].x(p[i].y()); trans_vt[i].y(p[i].z()); } } \
  else if (dominant_coord == 2) { trans_pt.x(q.x()); trans_pt.y(q.z()); for (int i=0;i<3;i++){ trans_vt[i].x(p[i].x()); trans_vt[i].y(p[i].z()); } } \
  else if (dominant_coord == 3 ) { trans_pt.x(q.x()); trans_pt.y(q.y()); for (int i=0;i<3;i++){ trans_vt[i].x(p[i].x()); trans_vt[i].y(p[i].y()); } } \
  for (int i = 0; i < 3; i++) trans_vt[i] -= trans_pt.asVector(); \
  int SH = 0, NSH = 0; double out_edge = 0.; \
  if (trans_vt[0].y() < 0.0) SH = -1; else SH = 1; \
  if (trans_vt[1].y() < 0.0) NSH = -1; else NSH = 1; \
  if (SH != NSH) { \
    if ( (trans_vt[0].x() > 0.0) && (trans_vt[1].x() > 0.0) ) NCS += 1; \
    else if ( (trans_vt[0].x() > 0.0) || (trans_vt[1].x() > 0.0) ) { \
      out_edge = (trans_vt[0].x() - trans_vt[0].y() * (trans_vt[1].x() - trans_vt[0].x())/(trans_vt[1].y() - trans_vt[0].y()) ); \
      if (out_edge == 0.0) { NES += 1; NCS += 1; } \
      if (out_edge > 0.0) NCS += 1; } \
    SH = NSH; } \
  if (trans_vt[2].y() < 0.0) NSH = -1; else NSH = 1; \
  if (SH != NSH) { \
    if ( (trans_vt[1].x() > 0.0) && (trans_vt[2].x() > 0.0) ) NCS += 1; \
    else if ( (trans_vt[1].x() > 0.0) || (trans_vt[2].x() >0.0) ) { \
      out_edge = (trans_vt[1].x() - trans_vt[1].y() * (trans_vt[2].x() -  trans_vt[1].x())/(trans_vt[2].y() - trans_vt[1].y()) ); \
      if (out_edge == 0.0){ NES += 1; NCS += 1; } \
      if (out_edge > 0.0) NCS +=1; } \
    SH = NSH; } \
  if (trans_vt[0].y() < 0.0) NSH = -1; else NSH = 1; \
  if ( SH != NSH) { \
    if ( (trans_vt[2].x() > 0.0) && (trans_vt[0].x() > 0.0) ) NCS += 1; \
    else if ( (trans_vt[2].x() > 0.0) || (trans_vt[0].x() >0.0) ) { \
      out_edge =  (trans_vt[2].x() - trans_vt[2].y() * (trans_vt[0].x() - trans_vt[2].x())/(trans_vt[0].y() - trans_vt[2].y()) ); \
      if (out_edge == 0.0) { NES +=1; NCS +=1; } \
      if (out_edge > 0.0) NCS += 1; } \
    SH = NSH; } \
} \
}

DEFINE_TRI(v_before)
DEFINE_TRI(v_after)

// ----- inside() VERBATIM (before): no bounding-box early-out -----
namespace v_before {
bool TriGeometryPiece::inside(const Point &p) const
{
  Vector infinity = Vector(-1e10,0.,0.) - p.asVector();
  int crossings = 0, NES = 0;
  for (int i = 0; i < (int) d_planes.size(); i++) {
    int NCS = 0;
    Point hit(0.,0.,0.);
    Plane plane = d_planes[i];
    int hit_me = plane.Intersect(p,infinity,hit);
    if (hit_me) {
      Vector int_ray = hit.asVector() - p.asVector();
      double cos_angle = Dot(infinity,int_ray)/
	(infinity.length()*int_ray.length());
      if (cos_angle < 0.)
	continue;
      insideTriangle(hit,i,NCS,NES);
      if (NCS % 2 != 0)
	crossings++;
      if (NES != 0)
	crossings -= NES/2;
    } else
      continue;
  }
  if (crossings%2 == 0)
    return false;
  else
    return true;
}
}

// ----- inside() VERBATIM (after): adds bounding-box early-out -----
namespace v_after {
bool TriGeometryPiece::inside(const Point &p) const
{
  // Check if Point p is outside the bounding box
  if (!(p == Max(p,d_box.lower()) && p == Min(p,d_box.upper())))
    return false;

  Vector infinity = Vector(-1e10,0.,0.) - p.asVector();
  int crossings = 0, NES = 0;
  for (int i = 0; i < (int) d_planes.size(); i++) {
    int NCS = 0;
    Point hit(0.,0.,0.);
    Plane plane = d_planes[i];
    int hit_me = plane.Intersect(p,infinity,hit);
    if (hit_me) {
      Vector int_ray = hit.asVector() - p.asVector();
      double cos_angle = Dot(infinity,int_ray)/
	(infinity.length()*int_ray.length());
      if (cos_angle < 0.)
	continue;
      insideTriangle(hit,i,NCS,NES);
      if (NCS % 2 != 0)
	crossings++;
      if (NES != 0)
	crossings -= NES/2;
    } else
      continue;
  }
  if (crossings%2 == 0)
    return false;
  else
    return true;
}
}

// ---------- Build a closed triangulated mesh (a box) ----------
template<class T>
void buildMesh(T& g){
  // 8 corners of a cube [-1,1]^3
  double c[8][3] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
  };
  for(int i=0;i<8;i++) g.d_points.push_back(Point(c[i][0],c[i][1],c[i][2]));
  // 12 triangles (2 per face)
  int tris[12][3] = {
    {0,1,2},{0,2,3}, // bottom z=-1
    {4,6,5},{4,7,6}, // top z=1
    {0,4,5},{0,5,1}, // y=-1
    {3,2,6},{3,6,7}, // y=1
    {0,3,7},{0,7,4}, // x=-1
    {1,5,6},{1,6,2}  // x=1
  };
  for(int i=0;i<12;i++) g.d_tri.push_back(IntVector(tris[i][0],tris[i][1],tris[i][2]));
  for(int i=0;i<12;i++){
    Point p0=g.d_points[g.d_tri[i].x()];
    Point p1=g.d_points[g.d_tri[i].y()];
    Point p2=g.d_points[g.d_tri[i].z()];
    g.d_planes.push_back(Plane(p0,p1,p2));
    Point mn=Min(Min(p0,p1),Min(p1,p2));
    Point mx=Max(Max(p0,p1),Max(p1,p2));
    g.d_boxes.push_back(Box(mn,mx));
  }
  Vector fudge(1e-5,1e-5,1e-5);
  Point mn(1e30,1e30,1e30),mx(-1e30,-1e30,-1e30);
  for(auto&pt:g.d_points){mn=Min(pt,mn);mx=Max(pt,mx);}
  g.d_box = Box(mn-fudge, mx-Vector(-1e-5,-1e-5,-1e-5)); // mx+fudge
}

int main(){
  v_before::TriGeometryPiece gb; buildMesh(gb);
  v_after::TriGeometryPiece  ga; buildMesh(ga);

  std::mt19937_64 rng(12345);
  std::uniform_real_distribution<double> big(-5.0,5.0);
  std::uniform_real_distribution<double> small(-1.2,1.2);

  // Diverse/boundary/adversarial battery
  std::vector<Point> tests;
  // grid over [-3,3]^3
  for(double x=-3;x<=3;x+=0.5)
   for(double y=-3;y<=3;y+=0.5)
    for(double z=-3;z<=3;z+=0.5)
      tests.push_back(Point(x,y,z));
  // boundary points near cube faces / edges / corners
  double b[]={-1.0,-1.00001,-0.99999,0.0,0.99999,1.0,1.00001};
  for(double x:b)for(double y:b)for(double z:b) tests.push_back(Point(x,y,z));
  // random inside-ish and far-outside
  for(int i=0;i<20000;i++) tests.push_back(Point(small(rng),small(rng),small(rng)));
  for(int i=0;i<20000;i++) tests.push_back(Point(big(rng),big(rng),big(rng)));

  long mism=0; Point firstMis(0,0,0); bool haveMis=false;
  for(auto&p:tests){
    bool rb=gb.inside(p);
    bool ra=ga.inside(p);
    if(rb!=ra){ if(!haveMis){firstMis=p;haveMis=true;} mism++; }
  }
  printf("total=%zu mismatches=%ld\n", tests.size(), mism);
  if(haveMis) printf("first_divergent=(%.6f,%.6f,%.6f)\n",firstMis.x_,firstMis.y_,firstMis.z_);

  // ---- Interleaved timing ----
  const int REP=200;
  std::vector<double> rb_ns, ra_ns;
  volatile long sink=0;
  for(int r=0;r<REP;r++){
    auto t0=std::chrono::high_resolution_clock::now();
    for(auto&p:tests){ sink+=gb.inside(p)?1:0; }
    auto t1=std::chrono::high_resolution_clock::now();
    for(auto&p:tests){ sink+=ga.inside(p)?1:0; }
    auto t2=std::chrono::high_resolution_clock::now();
    rb_ns.push_back(std::chrono::duration<double,std::nano>(t1-t0).count());
    ra_ns.push_back(std::chrono::duration<double,std::nano>(t2-t1).count());
  }
  std::sort(rb_ns.begin(),rb_ns.end());
  std::sort(ra_ns.begin(),ra_ns.end());
  double mb=rb_ns[REP/2], ma=ra_ns[REP/2];
  printf("median_before_ns=%.0f median_after_ns=%.0f speedup=%.3f sink=%ld\n", mb, ma, mb/ma, (long)sink);
  return 0;
}
