# gnb_udp_over_tcp
[GNB](https://github.com/gnbdev/gnb "GNB")是一个开源的去中心化的具有极致内网穿透能力的通过P2P进行三层网络交换的VPN。

GNB节点间通过UDP协议传输数据，在一些网络环境下的路由器/防火墙会对UDP分组实施QOS策略，因此通过tcp链路转发GNB数据是不需要改动GNB通讯协议又可以提升GNB网络适应能力的一个办法。

gnb_udp_over_tcp 是一个为GNB开发的通过tcp链路中转UDP分组转发的服务。

gnb_udp_over_tcp可以为其他基于UDP协议的服务中转数据。

# 用nc作本地测试演示

## Step1
用 nc 监听 7000 udp 端口

```sh
nc -u -l 7000
```

## Step2
启动 gnb_udp_over_tcp 的 tcp端: 监听 tcp 6000 端口，每个接入该端口的tcp链路将建立起一个udp socket构成一个channel，tcp链路收到的报文发往 127.0.0.1 的 UDP 7000端口，从udp端收到的数据将发往tcp链路的另一端。
gnb_udp_over_tcp的tcp端可以同时接入多个tcp连接并且转发到同一个目的地址的udp端口。

```sh
./gnb_udp_over_tcp -t -l 6000 127.0.0.1 7000
```


## Step3
启动 gnb_udp_over_tcp 的 udp端:  监听 udp 5001 端口，与 127.0.0.1 tcp 端口 6000 建立tcp链路，udp 端收到的数据发往tcp链路的另一端，从tcp链路收到的数据发往udp端。
```sh
./gnb_udp_over_tcp -u -l 5001 127.0.0.1 6000
```

## Step4
用 nc 访问 127.0.0.1 的 5001 udp 端口，检验数据是否被成功转发。

```sh
nc -u 127.0.0.1 5001
```

# 通过 gnb_udp_over_tcp 中继 GNB 数据

## 演示环境
远端GNB节点 ip地址 为 192.168.1.25
GNB UDP 端口 9025
GNB TUN ip 10.1.0.25
远端 GNB 配置文件不需要调整


中继服务器 ip地址为 192.168.1.11


在 192.168.1.11 上执行
```sh
./gnb_udp_over_tcp -t -l 6000 192.168.1.25 9025
```

本地ip地址为 192.168.1.10
GNB TUN ip 10.1.0.10


本地 GNB  conf/1010/address.conf 内容为
```
n|1025|127.0.0.1|5001
```

address.conf中不要配置 i 类型的GNB节点

在 192.168.1.10 上执行
```sh
./gnb_udp_over_tcp -u -l 5001 192.168.1.11 6000
```
启动本地的GNB节点后 ping 10.1.0.25 检验是否能够ping通。

---
[免责声明](docs/disclaimer.md)
