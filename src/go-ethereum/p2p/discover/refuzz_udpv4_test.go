package discover

import (
	"github.com/ethereum/go-ethereum/p2p/discover/v4wire"
	"github.com/ethereum/go-ethereum/p2p/enode"
	"testing"
)

// TestStartPingRefuzz 测试 start_ping_refuzz 函数
func TestStartPingRefuzz(t *testing.T) {
	// 创建一个 UDPv4 实例
	cfg := &Config{}
	cfg.withDefaults()
	// 创建一个 UDPv4 实例
	udp := &UDPv4{
		reqSend:    make(chan v4wire.Packet),
		reqReceive: make(chan v4wire.Packet),
		reqQueue:   make(chan uint64),
		log:        cfg.Log,
	}

	list := []*enode.Node{
		enode.MustParse("enode://ba85011c70bcc5c04d8607d3a0ed29aa6179c092cbdda10d5d32684fb33ed01bd94f588ca8f91ac48318087dcb02eaf36773a7a453f0eedd6742af668097b29c@10.0.1.16:30303?discport=30304"),
		enode.MustParse("enode://81fa361d25f157cd421c60dcc28d8dac5ef6a89476633339c5df30287474520caca09627da18543d9079b5b288698b542d56167aa5c09111e55acdbbdf2ef799@10.0.1.16:30303"),
		enode.MustParse("enode://9bffefd833d53fac8e652415f4973bee289e8b1a5c6c4cbe70abf817ce8a64cee11b823b66a987f51aaa9fba0d6a91b3e6bf0d5a5d1042de8e9eeea057b217f8@10.0.1.36:30301?discport=17"),
		enode.MustParse("enode://1b5b4aa662d7cb44a7221bfba67302590b643028197a7d5214790f3bac7aaa4a3241be9e83c09cf1f6c69d007c634faae3dc1b1221793e8446c0b3a09de65960@10.0.1.16:30303"),
	}

	udp.StartPingRefuzz(list)

}
