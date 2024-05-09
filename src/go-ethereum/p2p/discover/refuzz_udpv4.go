package discover

import (
	"fmt"
	"github.com/ethereum/go-ethereum/p2p/enode"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"time"
)

func (t *UDPv4) ReFuzz() {

	// 创建一个通道来接收文件内容
	contentChan := make(chan []*enode.Node)

	// 启动定时器，每隔 10 秒触发一次扫描文件夹
	ticker := time.Tick(10 * time.Second)

	// 启动一个 goroutine 定期扫描文件夹
	go func() {
		for range ticker {
			// 扫描文件夹并将文件内容发送到通道
			scanFolder("./node_data", contentChan)
		}
	}()

	// 从通道中接收文件内容
	nodeQueue := <-contentChan
	// star fuzz
	t.start_refuzz(nodeQueue)
}

func scanFolder(folderPath string, contentChan chan []*enode.Node) {
	// 检查文件夹是否存在
	_, err := os.Stat(folderPath)
	if os.IsNotExist(err) {
		// 如果文件夹不存在，则创建文件夹
		err := os.MkdirAll(folderPath, 0755)
		if err != nil {
			fmt.Println("Error creating folder:", err)
			return
		}
		fmt.Println("Folder created:", folderPath)
	}

	// 检查是否存在 1.txt 文件
	filePath := filepath.Join(folderPath, "1.txt")
	_, err = os.Stat(filePath)
	if !os.IsNotExist(err) {
		// 如果文件存在，则读取文件内容
		content, err := ioutil.ReadFile(filePath)
		node := make([]*enode.Node, 0)
		if err != nil {
			fmt.Println("Error reading file:", err)
			return
		}
		lines := strings.Split(string(content), "\n")

		// 遍历每一行内容
		for _, line := range lines {
			// 忽略空行
			if line == "" {
				continue
			}

			// 创建 enr.Record
			record, err := enode.ParseV4(line)
			if err != nil {
				fmt.Println("Failed to parse enode:", err)
				continue
			}
			// 在这里可以使用 node 进行后续处理，比如加入到节点池中等
			fmt.Println("Created enode:", record)

			node = append(node, record)
		}
		// 将文件内容发送到通道
		contentChan <- node
		return
	}

	//fmt.Println("1.txt file not found in", folderPath)
}

// start fuzzcode
func (t *UDPv4) start_refuzz(nodeQueue []*enode.Node) {
	for i := range nodeQueue {
		err := t.Ping(nodeQueue[i])
		if err != nil {
			// 处理错误，例如打印错误信息或采取其他措施
			fmt.Println("Error while pinging node:", err)
		}
	}
}
