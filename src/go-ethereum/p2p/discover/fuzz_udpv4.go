package discover

import (
	"encoding/json"
	"fmt"
	"github.com/ethereum/go-ethereum/p2p/discover/v4wire"
	"math"
	"math/rand"
	"os"
	"time"
)

const (
	pingFilename        = "ping.json"
	pongFilename        = "pong.json"
	findnodeFilename    = "findnode.json"
	neighborsFilename   = "neighbors.json"
	ENRRequestFilename  = "ENRRequest.json"
	ENRResponseFilename = "ENRResponse.json"
)

// Fuzzer 实现 fuzz 程序
type Fuzzer struct {
	// 追踪发送次数的 map
	PingSendCounts        map[string]uint64
	PongSendCounts        map[string]uint64
	FindNodeSendCounts    map[string]uint64
	NeighborsSendCounts   map[string]uint64
	ENRRequestSendCounts  map[string]uint64
	ENRResponseSendCounts map[string]uint64

	// 用于记录发送次数达到一定阈值时进行变异的策略
	MutationThresholds map[int]float64
}

// NewFuzzer 创建一个新的 fuzzer
func NewFuzzer() *Fuzzer {
	return &Fuzzer{
		PingSendCounts:        make(map[string]uint64),
		PongSendCounts:        make(map[string]uint64),
		FindNodeSendCounts:    make(map[string]uint64),
		NeighborsSendCounts:   make(map[string]uint64),
		ENRRequestSendCounts:  make(map[string]uint64),
		ENRResponseSendCounts: make(map[string]uint64),
		MutationThresholds:    map[int]float64{1000: 0.1, 5000: 0.2, 10000: 0.3},
	}
}

func (t *UDPv4) FuzzMsgs() {
	//TODO temp: send mutated tx here!

	time.Sleep(time.Duration(2) * time.Second) //sleep 2 minutes

	//t.Log().Error("Begin sending Fuzzed Transactions!!!!!!!")

	f := NewFuzzer()

	go t.SaveSeedLoop()
	//for {
	//	//p.Log().Warn("Sending Fuzzed Message!")
	//	MsgFuzzed(t, f)
	//	time.Sleep(time.Duration(1) * time.Second)
	//}
	//t.log.Warn("Start Fuzzing!!!")

	for {
		select {
		case req := <-t.reqSend:
			switch r := req.(type) {
			case *v4wire.Ping:
				// 处理 Ping 类型的 req
				// 例如：
				// 修改 r 的字段
				// 将修改后的 r 发送给 reqReceive
				// t.reqReceive <- r
				f.MutatePingMsg(r)
				t.reqReceive <- r
			case *v4wire.Pong:
				f.MutatePongMsg(r)
				t.reqReceive <- r
			case *v4wire.Findnode:
				f.MutateFindnodeMsg(r)
				t.reqReceive <- r
			case *v4wire.Neighbors:
				f.MutateNeighborsMsg(r)
			case *v4wire.ENRRequest:
				f.MutateENRRequestMsg(r)
				t.reqReceive <- r
			case *v4wire.ENRResponse:
				f.MutateENRResponseMsg(r)
				t.reqReceive <- r
			default:
				select {}
			}
		}
	}
}

func (t *UDPv4) SaveSeedLoop() {
	// 从t.reqQueue （chan uint64）通道中获取数据，获取到数据后将该数据追加写入保存到文件seed.out文件中
	fileName := "seed.out"
	file, err := os.OpenFile(fileName, os.O_APPEND|os.O_WRONLY|os.O_CREATE, 0644)
	if err != nil {
		fmt.Println("Error opening file:", err)
		return
	}
	defer file.Close()

	for {
		seed, ok := <-t.reqQueue
		if !ok {
			// 通道已关闭，退出循环
			break
		}

		_, err := fmt.Fprintf(file, "%d\n", seed)
		if err != nil {
			fmt.Println("Error writing to file:", err)
			continue
		}
	}
}

func (f *Fuzzer) MutatePingMsg(msg *v4wire.Ping) {
	// 更新发送次数
	f.PingSendCounts[msg.To.IP.String()]++

	//选择变异方法
	rand.Seed(time.Now().UnixNano())
	switch rand.Intn(3) {
	case 0:
		// 变异 Version 字段
		// 直接设置一个随机数或者极限值
		fuzzUint(&msg.Version)
	case 1:
		// 变异 Expiration 字段
		// 增加一个固定的值
		fuzzExpiration(&msg.Expiration, f.PingSendCounts[msg.To.IP.String()])
	case 2:
		// 同时变异 Version 和 Expiration 字段
		// 随机变异 Version 字段
		fuzzUint(&msg.Version)
		// 增加一个固定的值到 Expiration 字段
		fuzzExpiration(&msg.Expiration, f.PingSendCounts[msg.To.IP.String()])
	}

	//变异结束
	//err := SavePingToFile(msg, pingFilename)
	//if err != nil {
	//	fmt.Println("Error writing to file:", err)
	//}
}

func (f *Fuzzer) MutatePongMsg(msg *v4wire.Pong) {
	f.PongSendCounts[msg.To.IP.String()]++
	fuzzExpiration(&msg.Expiration, f.PongSendCounts[msg.To.IP.String()])
}

func (f *Fuzzer) MutateFindnodeMsg(msg *v4wire.Findnode) {
	//发送方式是调用 send()，可以在 refuzz 程序中进行重放
	//选择变异方法
	rand.Seed(time.Now().UnixNano())
	switch rand.Intn(3) {
	case 0:
		fuzzPubKey(&msg.Target)
	case 1:
		fuzzExpiration(&msg.Expiration)
	case 2:
		// 同时变异 Version 和 Expiration 字段
		// 随机变异 Version 字段
		fuzzPubKey(&msg.Target)
		// 增加一个固定的值到 Expiration 字段
		fuzzExpiration(&msg.Expiration)
	}
}

func (f *Fuzzer) MutateNeighborsMsg(msg *v4wire.Neighbors) {
	rand.Seed(time.Now().UnixNano())
	switch rand.Intn(3) {
	case 0:
		fuzzNodes(msg.Nodes)
	case 1:
		fuzzExpiration(&msg.Expiration)
	case 2:
		fuzzNodes(msg.Nodes)
		// 增加一个固定的值到 Expiration 字段
		fuzzExpiration(&msg.Expiration)
	}
}

func (f *Fuzzer) MutateENRRequestMsg(msg *v4wire.ENRRequest) {
	fuzzExpiration(&msg.Expiration)
}

func (f *Fuzzer) MutateENRResponseMsg(msg *v4wire.ENRResponse) {
	fuzzHash(msg.ReplyTok)
}

func fuzzUint(x *uint) {
	// 将 uint 类型的最大值定义为 max
	const max = math.MaxUint32
	// 将 uint 类型的最小值定义为 min
	const min = 0
	// 对 x 进行变异
	switch rand.Intn(3) {
	case 0:
		// 将 x 变异为 max
		*x = max
	case 1:
		// 将 x 变异为 min
		*x = min
	case 2:
		// 将 x 递增或递减
		if rand.Intn(2) == 0 {
			// 递增 x
			if *x < max {
				*x++
			}
		} else {
			// 递减 x
			if *x > min {
				*x--
			}
		}
	}
}

func fuzzUint64(x *uint64) {
	// 将 uint 类型的最大值定义为 max
	const max = math.MaxUint64
	// 将 uint 类型的最小值定义为 min
	const min = 0
	// 对 x 进行变异
	switch rand.Intn(3) {
	case 0:
		// 将 x 变异为 max
		*x = max
	case 1:
		// 将 x 变异为 min
		*x = min
	case 2:
		// 将 x 递增或递减
		if rand.Intn(2) == 0 {
			// 递增 x
			if *x < max {
				*x++
			}
		} else {
			// 递减 x
			if *x > min {
				*x--
			}
		}
	}
}

func fuzzExpiration(exp *uint64, args ...uint64) {
	// 将 uint 类型的最大值定义为 max
	const max = math.MaxUint64
	// 将 uint 类型的最小值定义为 min
	const min = 0
	// 增加一个固定的值
	var k uint64

	rand.Seed(time.Now().UnixNano())
	if len(args) == 1 {
		k = args[0]
	} else {
		k = rand.Uint64()
	}

	switch rand.Intn(3) {
	case 0:
		*exp = (*exp) + uint64(20*time.Second)*k // 假设增加的固定值为 100
	case 1:
		*exp = max
	case 2:
		*exp = min
	}
}

func fuzzHash(str []byte) {
	//v4中的数据包hash值

}

func fuzzPubKey(str *v4wire.Pubkey) {
	//限制为64位字符

}

func fuzzNodes(nodes []v4wire.Node) {
	//定制化的nodes返回
}

// SavePingToFile 函数将 Ping 结构体保存到 JSON 文件中
func SavePingToFile(ping *v4wire.Ping, filename string) error {
	// 打开或创建文件
	file, err := os.OpenFile(filename, os.O_APPEND|os.O_WRONLY|os.O_CREATE, 0644)
	if err != nil {
		return fmt.Errorf("error opening file: %v", err)
	}
	defer file.Close()

	// 将 Ping 结构体编码为 JSON 格式
	pingJSON, err := json.Marshal(ping)
	if err != nil {
		return fmt.Errorf("error marshaling ping to JSON: %v", err)
	}

	// 写入 JSON 数据到文件
	if _, err := file.Write(pingJSON); err != nil {
		return fmt.Errorf("error writing ping JSON to file: %v", err)
	}

	// 写入换行符，方便区分不同的 JSON 对象
	if _, err := file.WriteString("\n"); err != nil {
		return fmt.Errorf("error writing newline to file: %v", err)
	}

	return nil
}
