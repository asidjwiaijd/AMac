"""Small interactive USB maintenance menu; no SD-card removal required."""
from pathlib import Path
import subprocess
import sys
from serial.tools import list_ports
from mac_control import MacControl,ControlError

ROOT=Path(__file__).resolve().parents[1]

def battery_summary(state):
    if 'raw_charging' not in state:
        return '当前固件尚未提供独立充电诊断，请更新固件。'
    if not state.get('raw_samples'):
        return '等待电池采样，请稍后重试。'
    if state.get('raw_age_ms', 0)>5000:
        return '电池采样已过期，请稍后重试。'
    if not state.get('raw_valid'):
        return '电池电压无效，无法判断充电状态。'
    label={-1:'等待充电状态稳定',0:'未在充电',1:'正在充电'}.get(state['raw_charging'],'状态未知')
    return f"电池电压：{state['raw_mv']/1000:.3f} V；{label}。\n未在充电也可能是已充满；此硬件无法单独判断 USB 是否接入。"


def connect():
    ports=sorted(list_ports.comports(),key=lambda p:0 if p.vid==0x303a else 1)
    for port in ports:
        if (port.vid,port.pid) not in ((0x303a,0x1001),(0x1a86,0x55d3)): continue
        client=None
        try:
            client=MacControl(port.device,reset_on_failure=False);client.connect(timeout=2)
            return client
        except (OSError,ControlError):
            if client: client.close()
    raise ControlError('未找到设备。请接好 USB2 数据线并保持供电。')

def main():
    print('aMac USB 维护工具\n固件和 SD 文件均可通过线缆更新，不需要取出 SD 卡。')
    while True:
        print('\n1 查看卡内文件  2 复制文件到设备  3 备份文件到电脑\n4 进入维护设置  5 启动 Mac  6 鼠标状态  7 更新固件\n8 电池与充电状态  0 退出')
        choice=input('请选择：').strip()
        if choice=='0': return 0
        try:
            if choice=='7':
                subprocess.run([sys.executable,str(ROOT/'tools/amac_update.py')],cwd=ROOT,check=True)
                continue
            with connect() as client:
                status=client.boot_status()
                if choice in ('1','2','3','4') and status['phase']=='emulator':
                    print('请先在 Mac 的 Special 菜单中正常关机，再选择“进入维护设置”。');continue
                if choice=='4': client.boot_enter();print('已进入维护设置。');continue
                if choice in ('1','2','3') and status['phase']!='preboot':
                    print('请先选择“进入维护设置”。');continue
                if choice=='1':
                    for name in client.sd_list(): print(name)
                elif choice=='2':
                    source=Path(input('电脑文件完整路径：').strip().strip('"'))
                    name=input(f'卡内文件名（回车使用 {source.name}）：').strip() or source.name
                    last=[-1]
                    def progress(done,total):
                        percent=done*100//max(1,total)
                        if percent!=last[0] and percent%5==0:
                            print(f'传输 {percent}%（{done}/{total} 字节）',flush=True);last[0]=percent
                    print('使用临时文件和校验，原文件保留为 .previous；完整系统盘传输可能较久。')
                    result=client.sd_put(source,name,progress)
                    print(f"复制完成，已读回校验，共 {result['bytes']} 字节。")
                elif choice=='3':
                    name=input('卡内文件名：').strip()
                    output=Path(input('电脑保存完整路径：').strip().strip('"'))
                    result=client.sd_get(name,output)
                    print(f"备份完成，共 {result['bytes']} 字节，保存到 {output}。")
                elif choice=='5': client.boot_start();print('Mac 已开始启动。')
                elif choice=='8': print(battery_summary(client.power()))
                elif choice=='6':
                    fields=dict(word.split('=',1) for word in client.request('INPUT','OK INPUT').split()[3:])
                    print('鼠标：'+('已连接' if fields.get('mouse')=='1' else '未检测到')+
                          '；键盘：'+('已连接' if fields.get('keyboard')=='1' else '未检测到')+
                          '；鼠标输入报告：'+fields.get('reports','0')+' 个。')
        except (OSError,ValueError,ControlError,subprocess.CalledProcessError) as error:
            print('操作未完成：',error)

if __name__=='__main__':
    try: raise SystemExit(main())
    except (KeyboardInterrupt,EOFError): pass
