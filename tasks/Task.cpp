#include "Task.hpp"
#include <vizkit/EnvireVisualization.hpp>

#include <envire/tools/GraphViz.hpp>
#include <Eigen/Dense>

#include "PoseGraph.hpp"

using namespace graph_slam;

Task::Task(std::string const& name, TaskCore::TaskState initial_state)
    : TaskBase(name, initial_state), env(NULL), lastFeatureArrayValid( false )
{
}

Task::Task(std::string const& name, RTT::ExecutionEngine* engine, TaskCore::TaskState initial_state)
    : TaskBase(name, engine, initial_state), env(NULL), lastFeatureArrayValid( false )
{
}

Task::~Task()
{
}

void Task::odometry_delta_samplesTransformerCallback(const base::Time &ts, const ::base::samples::RigidBodyState &odometry_delta_samples_sample)
{
    // integrate the delta changes into the body2PrevBody
    envire::TransformWithUncertainty deltaBody2PrevBody( odometry_delta_samples_sample );
    body2PrevBody = deltaBody2PrevBody * body2PrevBody;
}

void Task::stereo_featuresTransformerCallback(const base::Time &ts, const ::stereo::StereoFeatureArray &feature_arrays_sample)
{
    lastFeatureArray = feature_arrays_sample;
    lastFeatureArrayValid = true;
}

void Task::distance_framesTransformerCallback(const base::Time &ts, const ::base::samples::DistanceImage &distance_frames_sample)
{
    // get the current transforms
    Eigen::Affine3d lcamera2body;
    if( !_lcamera2body.get( ts, lcamera2body ) )
	return;

    std::cerr << "### add node" << std::endl;

    const double error_offset = 0.01;
    body2PrevBody.setCovariance( body2PrevBody.getCovariance() +
	    Eigen::Matrix<double,6,6>::Identity() * error_offset );

    Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,6,6> > eigensolver( body2PrevBody.getCovariance() );
    std::cout << body2PrevBody.getCovariance() << std::endl;
    std::cout << "tests : " << std::endl
	    << "hermetian : " << body2PrevBody.getCovariance().isApprox( body2PrevBody.getCovariance().transpose() ) << std::endl
	    << "eigenvalues : " << eigensolver.eigenvalues().transpose() << std::endl;

    // initialize a new node, and add the sensor readings to it
    graph->initNode( body2PrevBody );
    graph->addSensorReading( distance_frames_sample, lcamera2body );
    if( lastFeatureArrayValid )
    {
	graph->addSensorReading( lastFeatureArray, lcamera2body );
	lastFeatureArrayValid = false;
    }

    graph->addNode();

    std::cerr << "add node done." << std::endl;

    // reset body2PrevBody
    body2PrevBody = envire::TransformWithUncertainty::Identity();
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See Task.hpp for more detailed
// documentation about them.

bool Task::configureHook()
{
    if (! TaskBase::configureHook())
        return false;

    env = new envire::Environment();
    if( _debug_viz.value() )
    {
	viz.start();
	viz.getWidget()->updateData( env );
    }

    graph = new PoseGraph( env );

    body2PrevBody = envire::TransformWithUncertainty::Identity();

    return true;
}

// bool Task::startHook()
// {
//     if (! TaskBase::startHook())
//         return false;
//     return true;
// }

void Task::updateHook()
{
    TaskBase::updateHook();
}

// void Task::errorHook()
// {
//     TaskBase::errorHook();
// }
// void Task::stopHook()
// {
//     TaskBase::stopHook();
// }

void Task::cleanupHook()
{
    TaskBase::cleanupHook();

    delete graph;
    delete env;
}

