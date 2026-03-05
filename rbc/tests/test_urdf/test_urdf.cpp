/**
 * @file test_urdf.cpp
 * @brief URDF 解析与模型测试教程
 * 
 * 【什么是 URDF？】
 * URDF (Unified Robot Description Format) 是 ROS 中用于描述机器人模型的 XML 格式。
 * 它定义了机器人的连杆(link)、关节(joint)、质量、惯性、碰撞体积等属性。
 * 
 * 【URDF 基本结构】
 * - <robot>: 根元素，包含整个机器人描述
 * - <link>: 定义一个连杆（刚体），包含视觉、碰撞、惯性属性
 * - <joint>: 定义关节，连接两个连杆，指定运动类型
 * - <material>: 定义材质/颜色
 * 
 * 【本教程内容】
 * 1. 如何创建一个简单的 URDF 字符串
 * 2. 如何解析 URDF XML
 * 3. 如何访问模型中的连杆、关节信息
 * 4. 如何处理不同类型的关节
 * 5. 如何获取惯性数据
 * 6. 如何处理错误情况
 */

#include "test_util.h"

#include <urdf_parser/urdf_parser.h>  // URDF 解析器头文件
#include <urdf_model/model.h>          // 机器人模型类
#include <urdf_model/link.h>           // 连杆类
#include <urdf_model/joint.h>          // 关节类
#include <console_bridge/console.h>    // 控制台日志（用于抑制错误输出）

#include <sstream>
#include <iostream>

namespace rbc {

/**
 * @brief 创建一个简单的 URDF 机器人 XML 字符串（双连杆机械臂）
 * 
 * 【机器人结构】
 * base_link (底座) --[旋转关节]--> arm_link (机械臂)
 * 
 * @return URDF XML 字符串
 * 
 * 【URDF 关键元素说明】
 * - <visual>: 视觉属性，用于显示
 * - <collision>: 碰撞属性，用于物理计算
 * - <inertial>: 惯性属性，包含质量和惯性张量
 * - <origin>: 相对父坐标系的变换（位置 xyz + 姿态 rpy）
 * - <limit>: 关节限位（角度/位置范围、力矩限制、速度限制）
 */
static std::string create_simple_urdf() {
    return R"(<?xml version="1.0"?>
<robot name="test_robot">
  <!-- 材质定义：蓝色 -->
  <material name="blue">
    <color rgba="0 0 0.8 1"/>
  </material>
  
  <!-- 
   * 底座连杆 base_link
   * 形状：长方体 0.5 x 0.5 x 0.1 米
   * 质量：1.0 kg
   -->
  <link name="base_link">
    <visual>
      <geometry>
        <box size="0.5 0.5 0.1"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <box size="0.5 0.5 0.1"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="1.0"/>
      <inertia ixx="0.01" ixy="0" ixz="0" iyy="0.01" iyz="0" izz="0.01"/>
    </inertial>
  </link>
  
  <!-- 
   * 机械臂连杆 arm_link
   * 形状：圆柱体，半径0.05米，长度1.0米
   * 视觉原点偏移：(0, 0, 0.5)，使圆柱从底部向上延伸
   * 质量：0.5 kg
   -->
  <link name="arm_link">
    <visual>
      <geometry>
        <cylinder radius="0.05" length="1.0"/>
      </geometry>
      <origin xyz="0 0 0.5" rpy="0 0 0"/>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.05" length="1.0"/>
      </geometry>
      <origin xyz="0 0 0.5" rpy="0 0 0"/>
    </collision>
    <inertial>
      <mass value="0.5"/>
      <origin xyz="0 0 0.5" rpy="0 0 0"/>
      <inertia ixx="0.05" ixy="0" ixz="0" iyy="0.05" iyz="0" izz="0.001"/>
    </inertial>
  </link>
  
  <!-- 
   * 关节：连接底座和机械臂
   * 类型：revolute（旋转关节）
   * 旋转轴：Z轴 (0, 0, 1)
   * 限位：-π 到 +π 弧度
   -->
  <joint name="base_to_arm" type="revolute">
    <parent link="base_link"/>
    <child link="arm_link"/>
    <origin xyz="0 0 0.05" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="10" velocity="1"/>
  </joint>
</robot>
)";
}

/**
 * @brief 创建一个具有多级关节层次的 URDF（3自由度机械臂）
 * 
 * 【机器人结构】
 * base --[连续旋转]--> link1 --[旋转]--> link2 --[移动]--> link3
 * 
 * 【关节类型说明】
 * - continuous: 连续旋转关节，无角度限制（如轮子）
 * - revolute: 旋转关节，有角度限制
 * - prismatic: 移动关节（棱柱关节），直线运动
 * 
 * @return URDF XML 字符串
 */
static std::string create_hierarchy_urdf() {
    return R"(<?xml version="1.0"?>
<robot name="hierarchy_robot">
  <!-- 根连杆：底座 -->
  <link name="base"/>
  
  <!-- 连杆1 -->
  <link name="link1"/>
  <!-- 
   * 关节1：连续旋转关节
   * 绕Z轴无限旋转
   -->
  <joint name="joint1" type="continuous">
    <parent link="base"/>
    <child link="link1"/>
    <origin xyz="0 0 1"/>
    <axis xyz="0 0 1"/>
  </joint>
  
  <!-- 连杆2 -->
  <link name="link2"/>
  <!-- 
   * 关节2：旋转关节
   * 绕Y轴旋转，范围 -90° 到 +90°
   -->
  <joint name="joint2" type="revolute">
    <parent link="link1"/>
    <child link="link2"/>
    <origin xyz="0 0 1"/>
    <axis xyz="0 1 0"/>
    <limit lower="-1.57" upper="1.57" effort="10" velocity="1"/>
  </joint>
  
  <!-- 连杆3 -->
  <link name="link3"/>
  <!-- 
   * 关节3：移动关节
   * 沿X轴直线移动，范围 0 到 0.5 米
   -->
  <joint name="joint3" type="prismatic">
    <parent link="link2"/>
    <child link="link3"/>
    <origin xyz="0 0 0.5"/>
    <axis xyz="1 0 0"/>
    <limit lower="0" upper="0.5" effort="10" velocity="0.1"/>
  </joint>
</robot>
)";
}

/**
 * 【测试套件】URDF 解析测试
 * 
 * 使用 doctest 框架进行单元测试
 * TEST_SUITE 定义测试套件，包含多个测试用例
 */
TEST_SUITE("urdf") {

    /**
     * 【测试用例1】解析简单 URDF
     * 
     * 学习目标：
     * 1. 如何使用 urdf::parseURDF() 解析 XML 字符串
     * 2. 如何获取机器人名称、连杆数量、关节数量
     * 3. 如何获取根连杆
     * 4. 如何通过名称获取特定连杆和关节
     * 5. 如何获取材质信息
     */
    TEST_CASE("parse_simple_urdf") {
        // 步骤1：创建 URDF XML 字符串
        auto xml_str = create_simple_urdf();
        
        // 步骤2：解析 URDF，返回 urdf::Model 智能指针
        // 解析成功返回有效指针，失败返回 nullptr
        auto model = urdf::parseURDF(xml_str);
        
        // 验证：模型是否成功创建
        CHECK(model != nullptr);
        
        if (model) {
            // 验证：机器人名称
            CHECK(model->getName() == "test_robot");
            
            // 验证：连杆数量（base_link, arm_link）
            CHECK(model->links_.size() == 2);
            
            // 验证：关节数量（base_to_arm）
            CHECK(model->joints_.size() == 1);
            
            // 【获取根连杆】
            // 根连杆是没有父关节的连杆，通常是机器人的固定底座
            auto root = model->getRoot();
            CHECK(root != nullptr);
            if (root) {
                CHECK(root->name == "base_link");
            }
            
            // 【通过名称获取连杆】
            // getLink() 返回指向 urdf::Link 的智能指针
            auto arm_link = model->getLink("arm_link");
            CHECK(arm_link != nullptr);
            if (arm_link) {
                CHECK(arm_link->name == "arm_link");
            }
            
            // 【通过名称获取关节】
            // getJoint() 返回指向 urdf::Joint 的智能指针
            // 可以访问关节的各种属性：
            // - type: 关节类型（REVOLUTE, PRISMATIC, CONTINUOUS 等）
            // - parent_link_name: 父连杆名称
            // - child_link_name: 子连杆名称
            auto joint = model->getJoint("base_to_arm");
            CHECK(joint != nullptr);
            if (joint) {
                CHECK(joint->name == "base_to_arm");
                CHECK(joint->type == urdf::Joint::REVOLUTE);  // 旋转关节
                CHECK(joint->parent_link_name == "base_link");
                CHECK(joint->child_link_name == "arm_link");
            }
            
            // 【获取材质】
            // getMaterial() 返回指向 urdf::Material 的智能指针
            // 材质包含颜色 rgba（红、绿、蓝、透明度）
            auto material = model->getMaterial("blue");
            CHECK(material != nullptr);
            if (material) {
                // 验证蓝色材质 (0, 0, 0.8, 1)
                CHECK(material->color.r == doctest::Approx(0.0f));   // R = 0
                CHECK(material->color.g == doctest::Approx(0.0f));   // G = 0
                CHECK(material->color.b == doctest::Approx(0.8f));   // B = 0.8
                CHECK(material->color.a == doctest::Approx(1.0f));   // A = 1（不透明）
            }
        }
    }

    /**
     * 【测试用例2】解析具有层次结构的 URDF
     * 
     * 学习目标：
     * 1. 理解机器人的树形结构
     * 2. 识别不同类型的关节
     * 3. 遍历连杆的子关节和子连杆
     */
    TEST_CASE("parse_hierarchy_urdf") {
        auto xml_str = create_hierarchy_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        CHECK(model != nullptr);
        
        if (model) {
            CHECK(model->getName() == "hierarchy_robot");
            
            // 验证：4个连杆（base, link1, link2, link3）
            CHECK(model->links_.size() == 4);
            
            // 验证：3个关节（joint1, joint2, joint3）
            CHECK(model->joints_.size() == 3);
            
            // 【验证不同关节类型】
            
            // joint1: 连续旋转关节（continuous）
            // 用于轮子等可以无限旋转的部件
            auto joint1 = model->getJoint("joint1");
            CHECK(joint1 != nullptr);
            if (joint1) {
                CHECK(joint1->type == urdf::Joint::CONTINUOUS);
            }
            
            // joint2: 旋转关节（revolute）
            // 有角度限制，如肘关节
            auto joint2 = model->getJoint("joint2");
            CHECK(joint2 != nullptr);
            if (joint2) {
                CHECK(joint2->type == urdf::Joint::REVOLUTE);
            }
            
            // joint3: 移动关节（prismatic）
            // 直线运动，如伸缩臂
            auto joint3 = model->getJoint("joint3");
            CHECK(joint3 != nullptr);
            if (joint3) {
                CHECK(joint3->type == urdf::Joint::PRISMATIC);
            }
            
            // 【遍历树形结构】
            // 每个连杆维护两个列表：
            // - child_joints: 从该连杆出发的关节
            // - child_links: 通过关节连接的子连杆
            auto root = model->getRoot();
            CHECK(root != nullptr);
            if (root) {
                CHECK(root->name == "base");
                // base 有一个子关节（joint1）
                CHECK(root->child_joints.size() == 1);
                // base 有一个子连杆（link1）
                CHECK(root->child_links.size() == 1);
            }
        }
    }

    /**
     * 【测试用例3】连杆惯性数据
     * 
     * 学习目标：
     * 1. 如何获取连杆的质量
     * 2. 如何获取惯性张量（inertia tensor）
     * 3. 如何获取惯性参考系的原点
     * 
     * 【惯性张量说明】
     * 惯性张量是一个 3x3 矩阵，描述刚体绕各轴旋转的阻力：
     * | ixx  ixy  ixz |
     * | ixy  iyy  iyz |
     * | ixz  iyz  izz |
     * 
     * 对角元素 ixx, iyy, izz 是绕 X/Y/Z 轴的转动惯量
     * 非对角元素是惯性积（通常对于对称物体为0）
     */
    TEST_CASE("link_inertial_data") {
        auto xml_str = create_simple_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        // REQUIRE 与 CHECK 的区别：
        // REQUIRE 失败会立即停止当前测试用例
        // CHECK 失败会继续执行
        REQUIRE(model != nullptr);
        
        // 【获取 base_link 的惯性数据】
        auto base_link = model->getLink("base_link");
        CHECK(base_link != nullptr);
        if (base_link && base_link->inertial) {
            // 验证质量
            CHECK(base_link->inertial->mass == doctest::Approx(1.0));
            
            // 验证惯性张量对角元素
            CHECK(base_link->inertial->ixx == doctest::Approx(0.01));
            CHECK(base_link->inertial->iyy == doctest::Approx(0.01));
            CHECK(base_link->inertial->izz == doctest::Approx(0.01));
        }
        
        // 【获取 arm_link 的惯性数据】
        auto arm_link = model->getLink("arm_link");
        CHECK(arm_link != nullptr);
        if (arm_link && arm_link->inertial) {
            // 验证质量
            CHECK(arm_link->inertial->mass == doctest::Approx(0.5));
            
            // 验证惯性原点位置
            // origin 是一个变换，包含 position（位置）和 rotation（旋转）
            CHECK(arm_link->inertial->origin.position.x == doctest::Approx(0.0));
            CHECK(arm_link->inertial->origin.position.y == doctest::Approx(0.0));
            CHECK(arm_link->inertial->origin.position.z == doctest::Approx(0.5));
        }
    }

    /**
     * 【测试用例4】关节限位
     * 
     * 学习目标：
     * 1. 如何获取关节的运动限位
     * 2. 理解 limit 的各个参数含义
     * 
     * 【关节限位参数】
     * - lower: 最小位置/角度
     * - upper: 最大位置/角度
     * - effort: 最大力/力矩限制
     * - velocity: 最大速度限制
     */
    TEST_CASE("joint_limits") {
        auto xml_str = create_simple_urdf();
        auto model = urdf::parseURDF(xml_str);
        
        REQUIRE(model != nullptr);
        
        auto joint = model->getJoint("base_to_arm");
        REQUIRE(joint != nullptr);
        REQUIRE(joint->limits != nullptr);
        
        // 验证位置限位：-π 到 +π 弧度（约 -180° 到 +180°）
        CHECK(joint->limits->lower == doctest::Approx(-3.14));
        CHECK(joint->limits->upper == doctest::Approx(3.14));
        
        // 验证力矩限制：10 N·m
        CHECK(joint->limits->effort == doctest::Approx(10.0));
        
        // 验证速度限制：1 rad/s
        CHECK(joint->limits->velocity == doctest::Approx(1.0));
    }

    /**
     * 【测试用例5】错误处理
     * 
     * 学习目标：
     * 1. 如何处理无效的 URDF 输入
     * 2. 如何抑制预期的错误日志输出
     * 3. 最小有效 URDF 的结构
     */
    TEST_CASE("invalid_urdf_handling") {
        // 【技巧：抑制错误日志】
        // console_bridge 用于输出解析错误信息
        // 在测试错误情况时，我们先保存当前日志级别，然后禁用日志
        // 这样可以避免测试输出被错误信息污染
        auto original_log_level = console_bridge::getLogLevel();
        console_bridge::setLogLevel(console_bridge::CONSOLE_BRIDGE_LOG_NONE);
        
        // 【测试1：空 XML】
        // 传入空字符串应该返回 nullptr
        {
            auto model = urdf::parseURDF("");
            CHECK(model == nullptr);
        }
        
        // 【测试2：无效 XML（缺少 robot 根元素）】
        // URDF 必须以 <robot> 为根元素
        {
            auto model = urdf::parseURDF("<invalid>xml</invalid>");
            CHECK(model == nullptr);
        }
        
        // 恢复原始日志级别
        console_bridge::setLogLevel(original_log_level);
        
        // 【测试3：最小有效 URDF】
        // 一个有效的 URDF 至少需要：
        // 1. XML 声明
        // 2. <robot> 根元素，带有 name 属性
        // 3. 至少一个 <link> 元素（这是树的根）
        // 注意：没有关节是合法的（机器人可以只有一个固定连杆）
        {
            std::string minimal = R"(<?xml version="1.0"?>
<robot name="minimal_robot">
  <link name="only_link"/>
</robot>
)";
            auto model = urdf::parseURDF(minimal);
            CHECK(model != nullptr);
            if (model) {
                CHECK(model->getName() == "minimal_robot");
                CHECK(model->links_.size() == 1);
                CHECK(model->joints_.size() == 0);
            }
        }
    }

}  // TEST_SUITE("urdf")

} // namespace rbc
