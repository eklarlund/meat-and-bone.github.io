#include <cfloat>
#include <iostream>
#include <math.h> 
#include <vector>

#include <Eigen/Dense>

#include <igl/adjacency_list.h>
#include <igl/adjacency_list.cpp>
#include <igl/per_vertex_normals.h>
#include <igl/read_triangle_mesh.h>
#include <igl/vertex_triangle_adjacency.h>
#include <igl/vertex_triangle_adjacency.cpp>
#include <igl/viewer/Viewer.h>
#include <igl/writeOBJ.h>

#include <meat_and_bone/interpolate.h>
#include <meat_and_bone/thicken.h>
#include <meat_and_bone/skirt.h>
#include <meat_and_bone/laplace_smooth.h>

/*
	Usage: Pressure-Pad -s inf displavement, 0 displacement, x intercept, scale factor, smooth factor

*/
double smoothF = 1; //Smoothing factor
double ymin=-.2, ymax=.3, xmin=3, xmax=4.2; //infDisp is asymtote as x->-inf, amp is distance between asymptotes xint is x intercept, expS is x scale
double displacement = .4, offset = -.2, drop = -.01, thickness = .5, solid_smooth = 1;

using namespace std;

void show_result(Eigen::MatrixXd V1, Eigen::MatrixXi F1, Eigen::MatrixXd V2, Eigen::MatrixXi F2)
{
	//Viewer
	std::cout << "\n\n-------------------Viewer--------------------\n\n";
	// Concatenate (V1,F1) and (V2,F2) into (Vv,Fv)
	Eigen::MatrixXd Vv(V1.rows() + V2.rows(), V1.cols());
	Vv << V1, V2;
	Eigen::MatrixXi Fv(F1.rows() + F2.rows(), F1.cols());
	Fv << F1, (F2.array() + V1.rows());
	Eigen::MatrixXd Vv_(Vv);
	Eigen::MatrixXi Fv_(Fv);
	// blue color for faces of first mesh, orange for second
	Eigen::MatrixXd Co(Fv.rows(), 3);
	Co <<
		Eigen::RowVector3d(0.2, 0.3, 0.8).replicate(F1.rows(), 1),
		Eigen::RowVector3d(1.0, 0.7, 0.2).replicate(F2.rows(), 1);

	igl::viewer::Viewer viewer;
	viewer.data.set_mesh(Vv_, Fv_);
	viewer.data.set_colors(Co);
	viewer.data.set_face_based(true);
	viewer.launch();
}
static void show_usage(string name)
{
	cerr << "Usage: " << name << " <option(s)> WORKSURFACE REFSURFACE RESSURFACE"
		<< "Options:\n"
		<< "\t-h,--help\t\tShow this help message\n"
		<< "\t-s,--scale SCALEFACTOR\tSpecify the scaling, default = 1"
		<< endl;
}


int main(int argc, char* argv[])
{
	if (argc < 3) {
		show_usage(argv[0]);
		return 1;
	}
	string work_surface;
	string ref_surface;
	string res_surface;
	double scale = 1;

	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		}
		else if ((arg == "-d") || (arg == "--sdbcale")) {
			if (i + 2 < argc) { //check for right # of inputs

				istringstream(argv[++i]) >> ymin;
				istringstream(argv[++i]) >> ymax;
			}
			else {
				cerr << "--scale option requires real number" << endl;
				show_usage(argv[0]);
				std::cout << "couldnt read y";
				return 1;
			}
		}
		else if ((arg == "-r"))
		{
			if (i + 2 < argc) { //check for right # of inputs
				istringstream(argv[++i]) >> xmin;
				istringstream(argv[++i]) >> xmax;
			}
			else {
				cerr << "--scale option requires real number" << endl;
				show_usage(argv[0]);
				std::cout << "couldnt read x";
				return 1;
			}
		}
		else if (arg == "-f")
		{
			if (i + 1 < argc) { //check for right # of inputs
				istringstream(argv[++i]) >> smoothF;
			}
			else {
				cerr << "--scale option requires real number" << endl;
				show_usage(argv[0]);
				std::cout << "Couldnt read sfactor";
				return 1;
			}
		}
		else if (arg == "-b") {
			if (i + 3 < argc) { //check for right # of inputs
				istringstream(argv[++i]) >> displacement;
				istringstream(argv[++i]) >> offset;
				istringstream(argv[++i]) >> drop;
			}
			else {
				cerr << "-b" << endl;
				show_usage(argv[0]);
				std::cout << "couldnt read x";
				return 1;
			}
		}
		//else if (arg == "-0") {
		//	if (i + 2 < argc) { //check for right # of inputs
		//		istringstream(argv[++i]) >> thickness;
		//		istringstream(argv[++i]) >> solid_smooth;
		//	}
		//	else {
		//		cerr << "--o" << endl;
		//		show_usage(argv[0]);
		//		std::cout << "couldnt read x";
		//		return 1;
		//	}
		//}

		else {
			if (i + 5 < argc)
			{
				std::cout << i + 5 << "   " << argc << endl;
				cerr << "Too few arguements\n";
				show_usage(argv[0]);
				return 1;
			}
			work_surface = argv[i++];
			cout << work_surface << endl;
			ref_surface = argv[i++];
			cout << ref_surface << endl;
			res_surface = argv[i++];
			cout << res_surface << endl;
		}
	}

	Eigen::MatrixXd V;
	Eigen::MatrixXi F;

	if (!igl::read_triangle_mesh(work_surface, V, F))
	{
		cerr << "file read error" << work_surface << endl;
		exit(1);
	}
	const int n = V.rows();

	Eigen::MatrixXd V_ref;
	Eigen::MatrixXi F_ref;

	if (!igl::read_triangle_mesh(ref_surface, V_ref, F_ref))
	{
		cerr << "file read error" << ref_surface << endl;
		exit(1);
	}

	int hits = 0, misses = 0;
	Eigen::MatrixXd N_smooth;
	laplace_smooth(V, F, N_smooth, smoothF);

	Eigen::VectorXd distances(N_smooth.rows());


	std::vector<std::vector<int> > adj;
	igl::adjacency_list(F, adj);

	find_distance(V, N_smooth, V_ref, F_ref, distances, hits, misses);//Comment out for speed
	interpolate_surfaces(adj, distances, misses); //Commented out for speed
	
	
	Eigen::MatrixXd V_orig(V);
	Eigen::MatrixXi F_orig(F);

	displace_vertices(V, N_smooth, distances, xmin, xmax, ymin, ymax); //Comment out for speed

	igl::writeOBJ("../displaced_surface", V, F);

	Eigen::VectorXi borderLoop(n); //border Vertices ordered in a loop
	Eigen::MatrixXd V_plus;
	Eigen::MatrixXi F_plus;


	if (make_skirt(V, F, N_smooth, V_plus, F_plus, borderLoop, 4, displacement, offset, drop)) 
		cout << "Make skirt worked?";
	
	cout << "\nFirst F:" << F_plus.row(1);
	Eigen::MatrixXd V_out;
	Eigen::MatrixXi F_out;
	if (make_solid(V_plus,F_plus, borderLoop, V_out, F_out, solid_smooth, thickness))
		cout << "\nmake_solid worked?";


	std::cout << "\nhits: " << hits << "\nmisses: " << misses << endl;
	igl::writeOBJ(res_surface, V_out, F_out);

   Eigen::MatrixXd V_plus_(V_out);
   Eigen::MatrixXi F_plus_(F_out);
   show_result(V_plus_, F_plus_, V_orig, F_orig);
  return 1;
}
