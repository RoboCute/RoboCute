/**
 * @file test_urdf_robo.cpp
 * @brief 基于 robo_doc.md 的机器人 URDF 测试案例
 * 
 * 【测试目标】
 * 本测试文件基于 robo_doc.md 中描述的机器人案例，创建具体的 URDF 测试场景。
 * 主要参考 myCobot 280 6自由度协作机械臂的结构进行建模。
 * 
 * 【myCobot 280 简介】
 * - 自由度：6 DOF
 * - 最大负载：250g
 * - 工作空间：280mm 臂展
 * - 重量：850g
 * 
 * 【机器人结构】
 * base_link -> joint1 -> link1 -> joint2 -> link2 -> joint3 -> link3
 *           -> joint4 -> link4 -> joint5 -> link5 -> joint6 -> link6 (end_effector)
 */

#include "test_util.h"

#include <urdf_parser/urdf_parser.h>
#include <urdf_model/model.h>
#include <urdf_model/link.h>
#include <urdf_model/joint.h>
#include <console_bridge/console.h>

#include <sstream>
#include <iostream>

namespace rbc {

/**
 * @brief 创建 myCobot 280 风格的 6 自由度机械臂 URDF
 * 
 * 【连杆结构】
 * - base_link: 底座，圆柱形，直径 0.08m，高度 0.05m
 * - link1: 旋转基座，圆柱形
 * - link2: 下臂，长方体
 * - link3: 上臂，长方体
 * - link4: 手腕旋转部，圆柱形
 * - link5: 手腕俯仰部，圆柱形
 * - link6: 末端执行器安装法兰，圆柱形
 * 
 * 【关节配置】
 * - joint1: 基座旋转，绕 Z 轴，-180° ~ +180°
 * - joint2: 肩关节，绕 Y 轴，-150° ~ +150°
 * - joint3: 肘关节，绕 Y 轴，-150° ~ +150°
 * - joint4: 手腕旋转，绕 Z 轴，-180° ~ +180°
 * - joint5: 手腕俯仰，绕 Y 轴，-100° ~ +100°
 * - joint6: 末端旋转，绕 Z 轴，-180° ~ +180°
 * 
 * @return URDF XML 字符串
 */
static std::string create_mycobot_280_urdf() {
    return R"(<?xml version="1.0"?>
<robot name="mycobot_280">
  <!-- 材质定义 -->
  <material name="white">
    <color rgba="1 1 1 1"/>
  </material>
  <material name="blue">
    <color rgba="0.2 0.4 0.8 1"/>
  </material>
  <material name="silver">
    <color rgba="0.75 0.75 0.75 1"/>
  </material>
  
  <!-- 底座连杆 -->
  <link name="base_link">
    <visual>
      <geometry>
        <cylinder radius="0.04" length="0.05"/>
      </geometry>
      <material name="silver"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.04" length="0.05"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.3"/>
      <inertia ixx="0.0001" ixy="0" ixz="0" iyy="0.0001" iyz="0" izz="0.0002"/>
    </inertial>
  </link>
  
  <!-- 连杆1：旋转基座 -->
  <link name="link1">
    <visual>
      <geometry>
        <cylinder radius="0.035" length="0.06"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.035" length="0.06"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.15"/>
      <inertia ixx="0.00005" ixy="0" ixz="0" iyy="0.00005" iyz="0" izz="0.0001"/>
    </inertial>
  </link>
  
  <!-- 连杆2：下臂 -->
  <link name="link2">
    <visual>
      <geometry>
        <box size="0.04 0.03 0.12"/>
      </geometry>
      <material name="white"/>
    </visual>
    <collision>
      <geometry>
        <box size="0.04 0.03 0.12"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.12"/>
      <origin xyz="0 0 0.06"/>
      <inertia ixx="0.00015" ixy="0" ixz="0" iyy="0.00018" iyz="0" izz="0.00005"/>
    </inertial>
  </link>
  
  <!-- 连杆3：上臂 -->
  <link name="link3">
    <visual>
      <geometry>
        <box size="0.035 0.025 0.10"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <box size="0.035 0.025 0.10"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.10"/>
      <origin xyz="0 0 0.05"/>
      <inertia ixx="0.00009" ixy="0" ixz="0" iyy="0.00011" iyz="0" izz="0.00003"/>
    </inertial>
  </link>
  
  <!-- 连杆4：手腕旋转部 -->
  <link name="link4">
    <visual>
      <geometry>
        <cylinder radius="0.025" length="0.04"/>
      </geometry>
      <material name="silver"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.025" length="0.04"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.08"/>
      <inertia ixx="0.00002" ixy="0" ixz="0" iyy="0.00002" iyz="0" izz="0.00003"/>
    </inertial>
  </link>
  
  <!-- 连杆5：手腕俯仰部 -->
  <link name="link5">
    <visual>
      <geometry>
        <cylinder radius="0.02" length="0.035"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.02" length="0.035"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.05"/>
      <inertia ixx="0.00001" ixy="0" ixz="0" iyy="0.00001" iyz="0" izz="0.00001"/>
    </inertial>
  </link>
  
  <!-- 连杆6：末端法兰 -->
  <link name="link6">
    <visual>
      <geometry>
        <cylinder radius="0.015" length="0.02"/>
      </geometry>
      <material name="silver"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.015" length="0.02"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.03"/>
      <inertia ixx="0.000003" ixy="0" ixz="0" iyy="0.000003" iyz="0" izz="0.000005"/>
    </inertial>
  </link>
  
  <!-- 关节1：基座旋转，绕 Z 轴 -->
  <joint name="joint1" type="revolute">
    <parent link="base_link"/>
    <child link="link1"/>
    <origin xyz="0 0 0.055" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="5" velocity="3.14"/>
  </joint>
  
  <!-- 关节2：肩关节，绕 Y 轴 -->
  <joint name="joint2" type="revolute">
    <parent link="link1"/>
    <child link="link2"/>
    <origin xyz="0 0 0.09" rpy="0 0 0"/>
    <axis xyz="0 1 0"/>
    <limit lower="-2.62" upper="2.62" effort="5" velocity="3.14"/>
  </joint>
  
  <!-- 关节3：肘关节，绕 Y 轴 -->
  <joint name="joint3" type="revolute">
    <parent link="link2"/>
    <child link="link3"/>
    <origin xyz="0 0 0.13" rpy="0 0 0"/>
    <axis xyz="0 1 0"/>
    <limit lower="-2.62" upper="2.62" effort="3" velocity="3.14"/>
  </joint>
  
  <!-- 关节4：手腕旋转，绕 Z 轴 -->
  <joint name="joint4" type="revolute">
    <parent link="link3"/>
    <child link="link4"/>
    <origin xyz="0 0 0.09" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="2" velocity="6.28"/>
  </joint>
  
  <!-- 关节5：手腕俯仰，绕 Y 轴 -->
  <joint name="joint5" type="revolute">
    <parent link="link4"/>
    <child link="link5"/>
    <origin xyz="0 0 0.04" rpy="0 0 0"/>
    <axis xyz="0 1 0"/>
    <limit lower="-1.75" upper="1.75" effort="2" velocity="6.28"/>
  </joint>
  
  <!-- 关节6：末端旋转，绕 Z 轴 -->
  <joint name="joint6" type="revolute">
    <parent link="link5"/>
    <child link="link6"/>
    <origin xyz="0 0 0.0375" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="1" velocity="6.28"/>
  </joint>
</robot>
)";
}

/**
 * @brief 创建移动机器人 URDF（差分驱动底盘）
 * 
 * 【机器人结构】
 * - base_link: 主体底盘，长方体
 * - left_wheel: 左驱动轮，圆柱形
 * - right_wheel: 右驱动轮，圆柱形
 * - caster_wheel: 万向轮（被动轮），球体
 * - lidar_link: 激光雷达安装位置
 * 
 * 【驱动方式】
 * - 左轮和右轮为连续旋转关节（continuous）
 * - 通过左右轮速差实现转向（差分驱动）
 * 
 * @return URDF XML 字符串
 */
static std::string create_mobile_robot_urdf() {
    return R"(<?xml version="1.0"?>
<robot name="mobile_robot">
  <!-- 材质定义 -->
  <material name="red">
    <color rgba="0.8 0.2 0.2 1"/>
  </material>
  <material name="black">
    <color rgba="0.1 0.1 0.1 1"/>
  </material>
  <material name="gray">
    <color rgba="0.5 0.5 0.5 1"/>
  </material>
  
  <!-- 底盘主体 -->
  <link name="base_link">
    <visual>
      <geometry>
        <box size="0.4 0.3 0.1"/>
      </geometry>
      <material name="red"/>
    </visual>
    <collision>
      <geometry>
        <box size="0.4 0.3 0.1"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="2.0"/>
      <inertia ixx="0.017" ixy="0" ixz="0" iyy="0.028" iyz="0" izz="0.042"/>
    </inertial>
  </link>
  
  <!-- 左轮 -->
  <link name="left_wheel">
    <visual>
      <geometry>
        <cylinder radius="0.05" length="0.04"/>
      </geometry>
      <material name="black"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.05" length="0.04"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.2"/>
      <inertia ixx="0.0002" ixy="0" ixz="0" iyy="0.0002" iyz="0" izz="0.00025"/>
    </inertial>
  </link>
  
  <!-- 右轮 -->
  <link name="right_wheel">
    <visual>
      <geometry>
        <cylinder radius="0.05" length="0.04"/>
      </geometry>
      <material name="black"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.05" length="0.04"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.2"/>
      <inertia ixx="0.0002" ixy="0" ixz="0" iyy="0.0002" iyz="0" izz="0.00025"/>
    </inertial>
  </link>
  
  <!-- 万向轮 -->
  <link name="caster_wheel">
    <visual>
      <geometry>
        <sphere radius="0.03"/>
      </geometry>
      <material name="gray"/>
    </visual>
    <collision>
      <geometry>
        <sphere radius="0.03"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.1"/>
      <inertia ixx="0.00004" ixy="0" ixz="0" iyy="0.00004" iyz="0" izz="0.00004"/>
    </inertial>
  </link>
  
  <!-- 激光雷达 -->
  <link name="lidar_link">
    <visual>
      <geometry>
        <cylinder radius="0.035" length="0.05"/>
      </geometry>
      <material name="gray"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.035" length="0.05"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.15"/>
      <inertia ixx="0.00005" ixy="0" ixz="0" iyy="0.00005" iyz="0" izz="0.00009"/>
    </inertial>
  </link>
  
  <!-- 左轮关节：连续旋转 -->
  <joint name="left_wheel_joint" type="continuous">
    <parent link="base_link"/>
    <child link="left_wheel"/>
    <origin xyz="0 0.17 0" rpy="-1.57 0 0"/>
    <axis xyz="0 0 1"/>
  </joint>
  
  <!-- 右轮关节：连续旋转 -->
  <joint name="right_wheel_joint" type="continuous">
    <parent link="base_link"/>
    <child link="right_wheel"/>
    <origin xyz="0 -0.17 0" rpy="-1.57 0 0"/>
    <axis xyz="0 0 1"/>
  </joint>
  
  <!-- 万向轮关节：固定 -->
  <joint name="caster_joint" type="fixed">
    <parent link="base_link"/>
    <child link="caster_wheel"/>
    <origin xyz="-0.15 0 -0.02"/>
  </joint>
  
  <!-- 激光雷达关节：固定 -->
  <joint name="lidar_joint" type="fixed">
    <parent link="base_link"/>
    <child link="lidar_link"/>
    <origin xyz="0.15 0 0.075"/>
  </joint>
</robot>
)";
}

/**
 * @brief 基于 robo_doc.md 的机器人 URDF 测试套件
 * 
 * 测试内容：
 * 1. myCobot 280 6自由度机械臂结构验证
 * 2. 移动机器人（差分驱动）结构验证
 * 3. 机器人运动学链验证
 */

    /**
     * 【测试用例1】myCobot 280 机械臂结构验证
     * 
     * 验证目标：
     * 1. 6自由度关节链正确连接
     * 2. 每个关节的类型和参数正确
     * 3. 连杆数量正确（7个：base + 6个连杆）
     */
    "mycobot_280_structure"_test = [] {
        auto xml_str = create_mycobot_280_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        expect(static_cast<bool>(model != nullptr)) << fatal;
        expect(static_cast<bool>(model->getName() == "mycobot_280"));
        
        // 验证连杆数量：base_link + link1~link6 = 7
        expect(static_cast<bool>(model->links_.size() == 7));
        
        // 验证关节数量：6个旋转关节
        expect(static_cast<bool>(model->joints_.size() == 6));
        
        // 验证根连杆
        auto root = model->getRoot();
        expect(static_cast<bool>(root != nullptr));
        if (root) {
            expect(static_cast<bool>(root->name == "base_link"));
        }
        
        // 验证所有连杆都存在
        expect(static_cast<bool>(model->getLink("base_link") != nullptr));
        expect(static_cast<bool>(model->getLink("link1") != nullptr));
        expect(static_cast<bool>(model->getLink("link2") != nullptr));
        expect(static_cast<bool>(model->getLink("link3") != nullptr));
        expect(static_cast<bool>(model->getLink("link4") != nullptr));
        expect(static_cast<bool>(model->getLink("link5") != nullptr));
        expect(static_cast<bool>(model->getLink("link6") != nullptr));
        
        // 验证所有关节都存在且类型正确
        for (int i = 1; i <= 6; ++i) {
            std::string joint_name = "joint" + std::to_string(i);
            auto joint = model->getJoint(joint_name);
            expect(static_cast<bool>(joint != nullptr));
            if (joint) {
                expect(static_cast<bool>(joint->type == urdf::Joint::REVOLUTE));
            }
        }
        
        // 验证关节链的父子关系
        auto joint1 = model->getJoint("joint1");
        expect(static_cast<bool>(joint1->parent_link_name == "base_link"));
        expect(static_cast<bool>(joint1->child_link_name == "link1"));
        
        auto joint6 = model->getJoint("joint6");
        expect(static_cast<bool>(joint6->parent_link_name == "link5"));
        expect(static_cast<bool>(joint6->child_link_name == "link6"));
    };

    /**
     * 【测试用例2】myCobot 280 关节限位验证
     * 
     * 验证目标：
     * 1. 关节1（基座旋转）限位：-180° ~ +180°
     * 2. 关节2/3（肩/肘关节）限位：-150° ~ +150°
     * 3. 关节5（手腕俯仰）限位：-100° ~ +100°
     */
    "mycobot_280_joint_limits"_test = [] {
        auto xml_str = create_mycobot_280_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        expect(static_cast<bool>(model != nullptr)) << fatal;
        
        // 关节1：-π ~ +π (180°)
        auto joint1 = model->getJoint("joint1");
        expect(static_cast<bool>(joint1 != nullptr)) << fatal;
        expect(static_cast<bool>(joint1->limits != nullptr)) << fatal;
        expect(static_cast<bool>(joint1->limits->lower == Approx(-3.14)));
        expect(static_cast<bool>(joint1->limits->upper == Approx(3.14)));
        expect(static_cast<bool>(joint1->limits->effort == Approx(5.0)));
        expect(static_cast<bool>(joint1->limits->velocity == Approx(3.14)));
        
        // 关节2：-2.62 ~ +2.62 rad (约150°)
        auto joint2 = model->getJoint("joint2");
        expect(static_cast<bool>(joint2 != nullptr)) << fatal;
        expect(static_cast<bool>(joint2->limits != nullptr)) << fatal;
        expect(static_cast<bool>(joint2->limits->lower == Approx(-2.62)));
        expect(static_cast<bool>(joint2->limits->upper == Approx(2.62)));
        
        // 关节5：-1.75 ~ +1.75 rad (约100°)
        auto joint5 = model->getJoint("joint5");
        expect(static_cast<bool>(joint5 != nullptr)) << fatal;
        expect(static_cast<bool>(joint5->limits != nullptr)) << fatal;
        expect(static_cast<bool>(joint5->limits->lower == Approx(-1.75)));
        expect(static_cast<bool>(joint5->limits->upper == Approx(1.75)));
    };

    /**
     * 【测试用例3】myCobot 280 材质验证
     * 
     * 验证目标：
     * 1. 三种材质定义正确
     * 2. 颜色值正确
     */
    "mycobot_280_materials"_test = [] {
        auto xml_str = create_mycobot_280_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        expect(static_cast<bool>(model != nullptr)) << fatal;
        
        // 验证白色材质
        auto white = model->getMaterial("white");
        expect(static_cast<bool>(white != nullptr));
        if (white) {
            expect(static_cast<bool>(white->color.r == Approx(1.0f)));
            expect(static_cast<bool>(white->color.g == Approx(1.0f)));
            expect(static_cast<bool>(white->color.b == Approx(1.0f)));
            expect(static_cast<bool>(white->color.a == Approx(1.0f)));
        }
        
        // 验证蓝色材质
        auto blue = model->getMaterial("blue");
        expect(static_cast<bool>(blue != nullptr));
        if (blue) {
            expect(static_cast<bool>(blue->color.r == Approx(0.2f)));
            expect(static_cast<bool>(blue->color.g == Approx(0.4f)));
            expect(static_cast<bool>(blue->color.b == Approx(0.8f)));
        }
        
        // 验证银色材质
        auto silver = model->getMaterial("silver");
        expect(static_cast<bool>(silver != nullptr));
        if (silver) {
            expect(static_cast<bool>(silver->color.r == Approx(0.75f)));
            expect(static_cast<bool>(silver->color.g == Approx(0.75f)));
            expect(static_cast<bool>(silver->color.b == Approx(0.75f)));
        }
    };

    /**
     * 【测试用例4】移动机器人结构验证
     * 
     * 验证目标：
     * 1. 差分驱动轮为 continuous 关节
     * 2. 传感器（激光雷达）为 fixed 关节
     * 3. 万向轮为 passive（fixed）关节
     */
    "mobile_robot_structure"_test = [] {
        auto xml_str = create_mobile_robot_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        expect(static_cast<bool>(model != nullptr)) << fatal;
        expect(static_cast<bool>(model->getName() == "mobile_robot"));
        
        // 验证连杆数量
        expect(static_cast<bool>(model->links_.size() == 5));
        
        // 验证关节数量
        expect(static_cast<bool>(model->joints_.size() == 4));
        
        // 验证驱动轮为 continuous 关节（差分驱动）
        auto left_wheel_joint = model->getJoint("left_wheel_joint");
        expect(static_cast<bool>(left_wheel_joint != nullptr));
        if (left_wheel_joint) {
            expect(static_cast<bool>(left_wheel_joint->type == urdf::Joint::CONTINUOUS));
            expect(static_cast<bool>(left_wheel_joint->parent_link_name == "base_link"));
            expect(static_cast<bool>(left_wheel_joint->child_link_name == "left_wheel"));
        }
        
        auto right_wheel_joint = model->getJoint("right_wheel_joint");
        expect(static_cast<bool>(right_wheel_joint != nullptr));
        if (right_wheel_joint) {
            expect(static_cast<bool>(right_wheel_joint->type == urdf::Joint::CONTINUOUS));
        }
        
        // 验证传感器为 fixed 关节
        auto lidar_joint = model->getJoint("lidar_joint");
        expect(static_cast<bool>(lidar_joint != nullptr));
        if (lidar_joint) {
            expect(static_cast<bool>(lidar_joint->type == urdf::Joint::FIXED));
            expect(static_cast<bool>(lidar_joint->parent_link_name == "base_link"));
            expect(static_cast<bool>(lidar_joint->child_link_name == "lidar_link"));
        }
        
        // 验证万向轮为 fixed 关节
        auto caster_joint = model->getJoint("caster_joint");
        expect(static_cast<bool>(caster_joint != nullptr));
        if (caster_joint) {
            expect(static_cast<bool>(caster_joint->type == urdf::Joint::FIXED));
        }
    };

    /**
     * 【测试用例5】机械臂惯性数据验证
     * 
     * 验证目标：
     * 1. 每个连杆都有惯性数据
     * 2. 质量值正确
     * 3. 惯性张量合理
     */
    "mycobot_280_inertial_data"_test = [] {
        auto xml_str = create_mycobot_280_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        expect(static_cast<bool>(model != nullptr)) << fatal;
        
        // 验证底座惯性
        auto base_link = model->getLink("base_link");
        expect(static_cast<bool>(base_link != nullptr));
        if (base_link && base_link->inertial) {
            expect(static_cast<bool>(base_link->inertial->mass == Approx(0.3)));
        }
        
        // 验证末端法兰惯性
        auto link6 = model->getLink("link6");
        expect(static_cast<bool>(link6 != nullptr));
        if (link6 && link6->inertial) {
            expect(static_cast<bool>(link6->inertial->mass == Approx(0.03)));
        }
        
        // 验证总质量（简单累加）
        double total_mass = 0.0;
        for (const auto& [name, link] : model->links_) {
            if (link && link->inertial) {
                total_mass += link->inertial->mass;
            }
        }
        // 0.3 + 0.15 + 0.12 + 0.10 + 0.08 + 0.05 + 0.03 = 0.83
        expect(static_cast<bool>(total_mass == Approx(0.83)));
    };

    /**
     * 【测试用例6】机器人运动学链遍历
     * 
     * 验证目标：
     * 1. 从根连杆可以遍历到所有子连杆
     * 2. 父子关系正确
     */
    "robot_kinematics_chain"_test = [] {
        auto xml_str = create_mycobot_280_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        expect(static_cast<bool>(model != nullptr)) << fatal;
        
        // 从根连杆开始遍历
        auto root = model->getRoot();
        expect(static_cast<bool>(root != nullptr)) << fatal;
        
        // base_link 应该有 1 个子关节（joint1）
        expect(static_cast<bool>(root->child_joints.size() == 1));
        expect(static_cast<bool>(root->child_links.size() == 1));
        
        if (!root->child_joints.empty()) {
            auto first_joint = root->child_joints[0];
            expect(static_cast<bool>(first_joint->name == "joint1"));
            
            // 通过 joint1 连接到 link1
            if (!root->child_links.empty()) {
                auto first_link = root->child_links[0];
                expect(static_cast<bool>(first_link->name == "link1"));
            }
        }
        
        // 验证 link6 是末端（没有子关节）
        auto link6 = model->getLink("link6");
        expect(static_cast<bool>(link6 != nullptr));
        if (link6) {
            expect(static_cast<bool>(link6->child_joints.empty()));
            expect(static_cast<bool>(link6->child_links.empty()));
        }
    };


} // namespace rbc