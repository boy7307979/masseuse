#include <ceres/ceres.h>
#include <ceres/local_parameterization.h>
#include <sophus/se3.hpp>
#include <sophus/so3.hpp>
#include <fstream>
#include <tuple>
#include <stdio.h>
#include <eigen3/Eigen/Core>
#include "CeresCostFunctions.h"
#include <ceres/normal_prior.h>
#include <unsupported/Eigen/MatrixFunctions>
#include "AutoDiffLocalParamSE3.h"
#include "TicToc.h"

using namespace std;

namespace Eigen{
    typedef Matrix<double, 6, 1> Vector6d;
    typedef Matrix<double, 6, 6> Matrix6d;
}

namespace masseuse {

class Factor;

// Convenience typedefs
typedef Sophus::SE3d Pose3;
typedef Sophus::SO3d Rot3;
typedef Eigen::Vector3d Point3;
typedef Eigen::MatrixXd Matrix;
typedef std::map<unsigned, Pose3> Values;
typedef std::vector<Factor> Graph;
typedef std::pair<std::shared_ptr<Graph>, std::shared_ptr<Values>>
GraphAndValues;

class Error {
  public:
  Error(){

  }

  Eigen::Vector3d& Translation(){
    return translation;
  }

  Eigen::Vector3d& Rotation(){
    return rotation;
  }

  double& MaxTransError(){
    return max_trans_error;
  }

  double& MaxRotError(){
    return max_rot_error;
  }

  unsigned& NumPoses(){
    return num_poses;
  }

  double& DistanceTraveled(){
    return distance_traveled;
  }

  double& PercentAvgTranslationError(){
    return percent_avg_trans_error;
  }

  double GetAverageTransError(){
    if(num_poses > 0){
      return translation.norm()/num_poses;
    }else{
      return -1;
    }
  }

  double GetPercentAverageTansError(){
    if(num_poses > 0){
      return percent_avg_trans_error/num_poses;
    }else{
      return -1;
    }
  }

  double GetAverageRotError(){
    if(num_poses > 0){
      return rotation.norm()/num_poses;
    }else{
      return -1;
    }
  }

  private:
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
    Eigen::Vector3d rotation = Eigen::Vector3d::Zero();
    double max_trans_error = 0;
    double max_rot_error = 0;
    unsigned num_poses = 0;
    double distance_traveled = 0;
    double percent_avg_trans_error = 0;
};

///////////////////////////////////////////////////////////////////////////////
class Factor {
  public:
  Factor(){
  }

  Factor(unsigned id1, unsigned id2, Pose3 rel, Matrix cov)
    : id1(id1), id2(id2), rel_pose(rel), cov(cov){
    switch_variable = 1.0;
  }

  unsigned id1;
  unsigned id2;
  Pose3 rel_pose;
  Matrix cov;
  bool isLCC = false;
  double switch_variable;
};

///////////////////////////////////////////////////////////////////////////////
class AbsPose {
 public:
  AbsPose(){
    id = -1;
    pose_vec.setZero();
    cov.setZero();
  }

  AbsPose(unsigned id, Eigen::Vector6d pose,
          Eigen::Matrix6d cov)
    : id(id), pose_vec(pose), cov(cov)
  {
  }

  unsigned id;
  Eigen::Vector6d pose_vec;
  Pose3 Twp;
  Eigen::Matrix6d cov;
};

///////////////////////////////////////////////////////////////////////////////
class RelPose {
 public:
  RelPose(){
    live_id = -1;
    ref_id = -1;
    rel_pose.setZero();
    cov.setZero();
    ext_id = -1;
  }

  RelPose(unsigned ref_id, unsigned live_id, Eigen::Vector6d rel_pose,
          Eigen::Matrix6d cov)
    : ref_id(ref_id), live_id(live_id), rel_pose(rel_pose), cov(cov)
  {
  }

  unsigned ext_id;
  unsigned ref_id;
  unsigned live_id;
  Eigen::Vector6d rel_pose;
  Eigen::Matrix6d cov;
};

///////////////////////////////////////////////////////////////////////////////
struct Options
{
  // I/O
  bool save_results_binary = false;
  string binary_output_path = "";

  // Debug
  bool print_error_statistics = true;
  bool print_full_report = true;
  bool print_brief_report = false;
  bool print_minimizer_progress = false;
  bool check_gradients = false;
  bool print_comparison_error_statistics = true;

  // Covariance tuning
  double rel_covariance_mult = 0.16;
  double cov_det_thresh = 5e-39;
  double cov_z_prior = 1e-3;
  bool use_identity_covariance = false;

  // Optimization switches
  bool enable_prior_at_origin = true;
  bool fix_first_pose = true;
  bool enable_z_prior = false;

  // Switchable Constraints
  bool enable_switchable_constraints = true;
  double switch_variable_prior_cov = 4e-4;

  // Ceres optimization options
  bool update_state_every_iteration = false;
  int num_iterations = 200;
  double absolute_error_tol = 0;
  double function_tolearnce = 1e-6; // default: 1e-6
  double gradient_tolerance = 1e-10; // default: 1e-10
  double parameter_tolerance= 1e-8;  // default: 1e-8

  // Huber loss delta parameter
  double huber_loss_delta = 1.0;
};

///////////////////////////////////////////////////////////////////////////////
class Masseuse
{
public:
    Masseuse() {}
    Masseuse(Options options);
    void LoadPosesFromFile(const string filename);
    void SaveAbsPG(string out_file);
    void SaveResultsG2o();
    const Values& GetValues();
    const Graph& GetGraph();
    const std::vector<AbsPose>& GetGroundTruth();
    const Values &GetComparisonValues();
    bool CalculateError(Error &error, const Values &values);
    void PrintErrorStatistics(const Values &values);
    void Relax();
    void LoadGroundTruth(const string& gt_file);
    void LoadPoseGraph(const string& pg_file);

    Options options;
private:
    GraphAndValues LoadPoseGraphAndLCC(const string& pose_graph_file,
                                       bool read_lcc);
    string KeyFromId(unsigned id);

    std::vector<RelPose> relative_poses;
    std::vector<AbsPose> output_abs_poses;
    std::map<string, RelPose> added_LCC;
    std::vector<RelPose> loop_closure_constraints;
    std::vector<AbsPose> gt_poses;
    Values comparison_values;
    Eigen::Vector6d origin;
    std::shared_ptr<Graph> graph;
    std::shared_ptr<Values> values;

}; //Masseuse

}
