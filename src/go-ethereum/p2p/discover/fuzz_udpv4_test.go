package discover

import (
	"encoding/hex"
	"fmt"
	"github.com/ethereum/go-ethereum/p2p/discover/v4wire"
	"github.com/ethereum/go-ethereum/rlp"
	"github.com/google/gofuzz"
	"net"
	"testing"
	"time"
)

func TestMutatePingMsg(t *testing.T) {
	// 创建一个 Ping 消息
	ping := &v4wire.Ping{
		Version:    1,
		From:       v4wire.Endpoint{IP: net.IP("127.0.0.1"), UDP: 30303, TCP: 30303},
		To:         v4wire.Endpoint{IP: net.IP("127.0.0.2"), UDP: 30303, TCP: 30303},
		Expiration: 1234567890,
		ENRSeq:     42,
		Rest:       []rlp.RawValue{[]byte{0x01, 0x02, 0x03}},
	}

	// 使用 gofuzz 创建一个 fuzzer
	f := fuzz.New().NilChance(0.1)

	fmt.Println("Version: ", ping.Version)
	// 对 Ping 消息进行变异
	MutatePingMsg(f, ping)
	fmt.Println("Version: ", ping.Version)

	// 在这里进行断言或其他需要的验证操作
	// 例如，检查变异后的 ping.Version 是否发生了变化
	if ping.Version == 1 {
		t.Error("Ping message version did not mutate")
	}
}

func TestMutateNeighborsMsg(t *testing.T) {
	// 创建一个 Neighbors 消息
	neighbors := &v4wire.Neighbors{
		Nodes: []v4wire.Node{
			{
				ID:  hexPubkey("3155e1427f85f10a5c9a7755877748041af1bcd8d474ec065eb33df57a97babf54bfd2103575fa829115d224c523596b401065a97f74010610fce76382c0bf32"),
				IP:  net.ParseIP("99.33.22.55").To4(),
				UDP: 4444,
				TCP: 4445,
			},
			{
				ID:  hexPubkey("312c55512422cf9b8a4097e9a6ad79402e87a15ae909a4bfefa22398f03d20951933beea1e4dfa6f968212385e829f04c2d314fc2d4e255e0d3bc08792b069db"),
				IP:  net.ParseIP("1.2.3.4").To4(),
				UDP: 1,
				TCP: 1,
			},
			{
				ID:  hexPubkey("38643200b172dcfef857492156971f0e6aa2c538d8b74010f8e140811d53b98c765dd2d96126051913f44582e8c199ad7c6d6819e9a56483f637feaac9448aac"),
				IP:  net.ParseIP("2001:db8:3c4d:15::abcd:ef12"),
				UDP: 3333,
				TCP: 3333,
			},
			{
				ID:  hexPubkey("8dcab8618c3253b558d459da53bd8fa68935a719aff8b811197101a4b2b47dd2d47295286fc00cc081bb542d760717d1bdd6bec2c37cd72eca367d6dd3b9df73"),
				IP:  net.ParseIP("2001:db8:85a3:8d3:1319:8a2e:370:7348"),
				UDP: 999,
				TCP: 1000,
			},
		},
		Expiration: 1136239445,
		Rest:       []rlp.RawValue{{0x01}, {0x02}, {0x03}},
	}

	// 使用 gofuzz 创建一个 fuzzer
	f := fuzz.New().NilChance(0.1)

	fmt.Println("Expiration: ", neighbors.Expiration)
	MutateNeighborsMsg(f, neighbors)
	fmt.Println("Expiration: ", neighbors.Expiration)
	for i, node := range neighbors.Nodes {
		fmt.Printf("Node %d:\n", i+1)
		fmt.Printf("  ID: %x\n", node.ID)
		fmt.Printf("  IP: %s\n", node.IP.String())
		fmt.Printf("  UDP: %d\n", node.UDP)
		fmt.Printf("  TCP: %d\n", node.TCP)
		fmt.Println()
	}
}

func TestFuzzMsgs(t *testing.T) {
	// 创建一个 UDPv4 实例
	cfg := &Config{}
	cfg.withDefaults()

	udp := &UDPv4{
		reqSend:    make(chan v4wire.Packet),
		reqReceive: make(chan v4wire.Packet),
		log:        cfg.Log,
	}

	// 启动模拟发送和变异消息的 goroutine
	go udp.FuzzMsgs()

	// 创建一个 Ping 消息
	ping := &v4wire.Ping{
		Version:    1,
		From:       v4wire.Endpoint{IP: net.IP("127.0.0.1"), UDP: 30303, TCP: 30303},
		To:         v4wire.Endpoint{IP: net.IP("127.0.0.2"), UDP: 30303, TCP: 30303},
		Expiration: 1234567890,
		ENRSeq:     42,
		Rest:       []rlp.RawValue{[]byte{0x01, 0x02, 0x03}},
	}

	// 将 Ping 消息发送到 reqSend 通道
	udp.reqSend <- ping

	// 等待一段时间以确保消息被处理
	time.Sleep(time.Second)

	// 从 reqReceive 通道接收变异后的消息
	receivedMsg := <-udp.reqReceive

	// 检查接收到的消息是否是变异后的 Ping 消息
	receivedPing, ok := receivedMsg.(*v4wire.Ping)
	if !ok {
		t.Error("Received message is not of type *v4wire.Ping")
	}

	// 在这里进行断言或其他需要的验证操作
	// 例如，检查变异后的 ping.Version 是否发生了变化
	if receivedPing.Version == 1 {
		t.Error("Ping message version did not mutate")
	}
}

func hexPubkey(h string) (ret v4wire.Pubkey) {
	b, err := hex.DecodeString(h)
	if err != nil {
		panic(err)
	}
	if len(b) != len(ret) {
		panic("invalid length")
	}
	copy(ret[:], b)
	return ret
}
