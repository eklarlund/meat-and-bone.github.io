//#include <Eigen/core>
#include "laplace_smooth.h"
#include <iostream>

using namespace std;

bool make_solid(
	const Eigen::MatrixXd &V,
	const Eigen::MatrixXi &F,
	const Eigen::VectorXi &borderLoop,
	Eigen::MatrixXd & V_out,
	Eigen::MatrixXi & F_out,
	const double smoothf,
	const double offset) {

	const int n = V.rows();
	const int num_faces = F.rows();
	const int num_border_v = borderLoop.rows();
	Eigen::MatrixXd N;
	laplace_smooth(V, F, N, smoothf);

	Eigen::MatrixXd V_new = V;
	Eigen::MatrixXi F_new = F;

	V_out.resize(2 * V.rows(), V.cols());
	F_out.resize(2 * F.rows(), F.cols());

	for (int i = 0; i < n; i++) {
		V_new.row(i) = V.row(i) - offset * N.row(i);
	}

	for (int i = 0; i < num_faces; i++) {
		for (int j = 0; j < F.cols(); j++) {
			F_new(i, j) = F(i, 2 - j);
		}
	}

	V_out << V, V_new;
	if (offset > 0)
		F_out << F, (F_new.array() + V.rows());
	else
		F_out << F_new, (F.array() + V.rows());
	
	int f_curr = F_out.rows();
	F_out.conservativeResize(f_curr + 2*num_border_v, F_out.cols());
	const int V_n = V.rows();

	for (int i = 0; i < num_border_v; i++) {
		int next_i = (i + 1) % num_border_v;
		Eigen::Vector3i new_face_1(borderLoop(next_i), borderLoop(i), borderLoop(i) + n);
		Eigen::Vector3i new_face_2(borderLoop(next_i) + n, borderLoop(next_i), borderLoop(i) + n);
		F_out.row(f_curr++) = new_face_1;
		F_out.row(f_curr++) = new_face_2;
	}

	return 1;
}


