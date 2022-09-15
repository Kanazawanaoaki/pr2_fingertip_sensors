#!/usr/bin/env python

import rospy
from pr2_msgs.msg import PressureState
from pr2_fingertip_sensors.msg import PR2FingertipSensor


class ParsePFS(object):
    def __init__(self):
        self.grippers = ['l_gripper', 'r_gripper']
        self.fingertips = ['l_fingertip', 'r_fingertip']
        # Publishers
        self.pub = {}
        for gripper in self.grippers:
            self.pub[gripper] = {}
            for fingertip in self.fingertips:
                self.pub[gripper][fingertip] = rospy.Publisher(
                    '/pfs/' + gripper + '/' + fingertip, PR2FingertipSensor,
                    queue_size=1)
        # Subscribers
        rospy.Subscriber(
            "/pressure/l_gripper_motor", PressureState, self.cb, "l_gripper")
        rospy.Subscriber(
            "/pressure/r_gripper_motor", PressureState, self.cb, "r_gripper")
        # PFS data to be parsed
        self.pfs_data = {}
        for gripper in self.grippers:
            self.pfs_data[gripper] = {}
            for fingertip in self.fingertips:
                self.pfs_data[gripper][fingertip] = {}
                for packet in [0, 1]:
                    self.pfs_data[gripper][fingertip][packet] = None

    def cb(self, msg, gripper):
        """
        Collect sensor data to self.pfs_data.
        If sensor data of all fingers are collected (all data are not None),
        publish topic
        """
        # Parse input rostopic
        for fingertip in self.fingertips:
            if fingertip == 'l_fingertip':
                data = self.parse(msg.l_finger_tip)
            if fingertip == 'r_fingertip':
                data = self.parse(msg.r_finger_tip)
            # Store parsed sensor data
            self.pfs_data[gripper][fingertip][data['footer']] = data
            # If all sensor data is stored, append them
            if self.pfs_data[gripper][fingertip][0] is not None \
               and self.pfs_data[gripper][fingertip][1] is not None:
                # Create method?
                header = msg.header
                prox = self.pfs_data[gripper][fingertip][0]['proximity'] + \
                    self.pfs_data[gripper][fingertip][1]['proximity']
                force = self.pfs_data[gripper][fingertip][0]['force'] + \
                    self.pfs_data[gripper][fingertip][1]['force']
                acc = self.pfs_data[gripper][fingertip][0]['imu']
                gyro = self.pfs_data[gripper][fingertip][1]['imu']
                self.publish(
                    gripper, fingertip, header, prox, force, acc, gyro)
                # Reset self.pfs_data to None after publish
                self.pfs_data[gripper][fingertip][0] = None
                self.pfs_data[gripper][fingertip][1] = None

    def publish(self, gripper, fingertip, header, proximity, force, acc, gyro):
        """
        Publish parsed sensor data
        """
        pfs_msg = PR2FingertipSensor()
        pfs_msg.header = header
        pfs_msg.proximity = proximity
        pfs_msg.force = force
        pfs_msg.imu.linear_acceleration.x = acc[0]
        pfs_msg.imu.linear_acceleration.y = acc[1]
        pfs_msg.imu.linear_acceleration.z = acc[2]
        pfs_msg.imu.angular_velocity.x = gyro[0]
        pfs_msg.imu.angular_velocity.y = gyro[1]
        pfs_msg.imu.angular_velocity.z = gyro[2]
        self.pub[gripper][fingertip].publish(pfs_msg)

    def parse(self, packet):
        """
Args: packet is int16[22] array
Return: prox[12], force[12], imu[3](acc/gyro), footer, check_sum

c.f.
parse input
$ rosmsg show pr2_msgs/PressureState
std_msgs/Header header
  uint32 seq
  time stamp
  string frame_id
int16[] l_finger_tip
int16[] r_finger_tip

parse output
$ rosmsg show pr2_fingertip_sensors/PR2FingertipSensor
std_msgs/Header header
  uint32 seq
  time stamp
  string frame_id
int16[] proximity
int16[] force
sensor_msgs/Imu imu

packet protocol
https://docs.google.com/presentation/d/1VxRJWDqeDk_ryu-x1Vhj3_6BDu3gscwvNpngHKwfR4M/edit#slide=id.g1585f6b098c_0_0
        """
        # int16[] to binary
        packet_bin = ''
        for p in packet:
            # Apply mask to accept negative numbers
            packet_bin += '{0:0=16b}'.format(p & 0b1111111111111111)
        # Proximity and Force sensor
        prox = []
        force = []
        for i in range(12):
            prox_bin = packet_bin[i*24:i*24+12]
            prox_int = int(prox_bin, 2)
            prox.append(prox_int)
            force_bin = packet_bin[i*24+12:i*24+24]
            force_int = int(force_bin, 2)
            force.append(force_int)
        # imu
        imu = []
        for i in range(3):
            imu_bin = packet_bin[288+i*16:288+i*16+16]
            imu_int = int(imu_bin, 2)
            imu.append(imu_int)
        # footer
        footer_bin = packet_bin[336:344]
        footer = int(footer_bin, 2)
        # check_sum
        check_sum_bin = packet_bin[344:352]
        check_sum = int(check_sum_bin, 2)
        # calculate check_sum in this node
        sum_data = 0
        for i in range(1, 42, 2):
            packet_uint8 = int(packet_bin[i*8:i*8+8], 2)
            sum_data += packet_uint8
        sum_data = sum_data % 256
        if check_sum == sum_data:
            rospy.loginfo(
                'check_sum ({}) is correctly calculated.'.format(check_sum))
        else:
            rospy.logerr(
                'check_sum ({}) is different from calculation ({}).'.format(
                    check_sum, sum_data))
            raise AssertionError
        return {'proximity': prox,
                'force': force,
                'imu': imu,
                'footer': footer,
                'check_sum': check_sum}


if __name__ == '__main__':
    rospy.init_node('parse_pfs')
    pp = ParsePFS()
    rospy.spin()
