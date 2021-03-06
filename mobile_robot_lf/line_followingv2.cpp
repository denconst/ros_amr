#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <sstream>
#include <math.h>
#include <string>
#include <geometry_msgs/Twist.h>


  cv::Mat gray, blank;
  cv::Mat edge_detect;


  //Canny threshold info: for small ROI it worked well with lowthresh being 70 and the highthresh being 140, fullview 70low 210high
  int lowthreshold =70; //79 was tested through use of trackbar
  double minx1 =640, maxx2 =0;
  double error = 0;
  double xloc = 320;
  double base = 10,maxspeed=50;     
  double diff = 0;
  double P=0;
  double kp = 0.12;//0.1;
  int denom =0;
  std::string word =" ";
  ros::Publisher pub;
  double maxangle,minangle,absangle=0;
  std::ostringstream os;



void CallBackFuncMono(const sensor_msgs::ImageConstPtr& frame)
{
  cv_bridge::CvImagePtr cv_ptr;
  try
  {
   cv_ptr  = cv_bridge::toCvCopy(frame, sensor_msgs::image_encodings::MONO8);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
  }
  cv::Mat view= cv_ptr->image;
  cv::cvtColor(view,view, CV_GRAY2BGR);
  cv::Mat ROI = cv_ptr->image(cv::Rect(0,430,640,50));
  
////////////////////////////////ROI edge detection//////////////////////
 //cv::Mat blank(480,50,CV_8UC1,0);

  cv::blur(ROI,gray,cv::Size(3,3));
  cv::Canny(gray,edge_detect, lowthreshold, lowthreshold*3, 3);

  //gray.copyTo(blank, edge_detect);

 // cv::cvtColor(blank,blank, CV_GRAY2BGR);
  
  std::vector<cv::Vec4i> lines;
  cv::HoughLinesP(edge_detect, lines, 1, CV_PI/180,10,10,20);

  minx1 = 640;
  maxx2 = 0;
  for( size_t i = 0; i < lines.size(); i++)
  {
    cv::Vec4i l = lines[i];
    if(l[0] < minx1){
     minx1= l[0];
    }
    if(l[2] > maxx2){
     maxx2 = l[2];
    }
   // cv::line(blank, cv::Point(l[0],l[1]),cv::Point(l[2],l[3]),cv::Scalar(0,0,255), 3, CV_AA);
  }
  if(minx1!=640 && maxx2 !=0){
   xloc = (((maxx2-minx1)/2)+minx1);
  }
  error = 320-xloc;

///////////////////////////////ROI edge detection END///////////////////


////////////////////////////////Full image edge detection////////////////      
 // cv::Mat blank2(480,640,CV_8UC1,0);

  cv::blur(cv_ptr->image,gray,cv::Size(3,3));
  cv::Canny(gray,edge_detect, lowthreshold, lowthreshold*3, 3);

 // gray.copyTo(blank2, edge_detect);

 // cv::cvtColor(blank2,blank2, CV_GRAY2BGR);
  maxangle = 0;
  minangle = 180;
  std::vector<cv::Vec4i> linesfull;
  cv::HoughLinesP(edge_detect, linesfull, 1, CV_PI/180,50,40,30);
  for( size_t i = 0; i < linesfull.size(); i++)
  {
    cv::Vec4i l = linesfull[i];
    cv::line(view, cv::Point(l[0],l[1]),cv::Point(l[2],l[3]),cv::Scalar(0,0,255), 3, CV_AA);
 // displays angle of line - vertical = 90, horizontal =0,180
    double Angle = atan2 (l[3] - l[1],l[2] - l[0]) * 180.0 / CV_PI;
    if(Angle<0) Angle=Angle+360;
    if(Angle>269) Angle-=180;
    if(Angle>maxangle) maxangle = Angle;
    if(Angle<minangle) minangle = Angle;
  }   
///////////////////////////////Full image edge detection END///////////////

///////////////////////////////Base speed adjustment///////////////////////
  if(maxangle!=0 && minangle!=180){
    if((maxangle-90)>(90-minangle)){
      absangle = maxangle -90;
    }
    else{
      absangle = 90 - minangle;
    }
  }
  base = maxspeed - (absangle*1.17);  //1.66 for 30deg(best for top =60)-top50->30deg/1.17-40deg/.875-50deg/0.7
  if(base<15) base = 15;     

///////////////////////////////Base speed adjustment END///////////////////

//////////////////////////////Twist message publisher / PID ///////////////


    geometry_msgs::Twist msg;
    P = error*kp;
    diff = P;
    msg.linear.x=base;
    msg.angular.z=diff;
   
    pub.publish(msg);

 ///////////////////////////Twist message END /////////////////////////////
    cv::cvtColor(ROI,ROI, CV_GRAY2BGR);
    cv::circle(ROI, cv::Point((xloc),25),2,cv::Scalar(255,255,255),3);
    cv::line(ROI, cv::Point(320,50),cv::Point(320,0),cv::Scalar(0,0,255), 1, CV_AA);
    denom++;
    if((denom%5) == 0){
    std::ostringstream os;
    os << "Max angle: " << absangle << "        RPM: " << base << std::endl;
    word = os.str();
    denom = 0;
    }
    cv::putText(view,word,cv::Point(0,420),CV_FONT_HERSHEY_SIMPLEX,1,cv::Scalar(255,0,0),2);
    cv::imshow("MRLF" ,ROI);
    cv::imshow("full" ,view);
    cv::waitKey(1);
}




int main(int argc, char **argv)
{
  
  ros::init(argc,argv,"line_follower");
  ros::NodeHandle nh;
  cv::namedWindow("MRLF");
  cv::namedWindow("full");
  pub=nh.advertise<geometry_msgs::Twist>("/cmd_vel",3);

  image_transport::ImageTransport it(nh);
  image_transport::Subscriber sub = it.subscribe("camera/rgb/image_mono", 1, CallBackFuncMono);

  ros::spin();

  cv::destroyAllWindows();
}
