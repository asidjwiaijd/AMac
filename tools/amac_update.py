"""USB2 application update, with revision/hash guards and a verified rollback copy."""
from pathlib import Path
import argparse
from datetime import datetime
import hashlib
import contextlib
import io
import json
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.pio-core/tools/esptool-5.3.1'))
from mac_control import MacControl, ControlError


def load_application(image):
    metadata=json.loads(image.with_suffix('.json').read_text(encoding='utf-8'))
    merged=image.read_bytes()
    if hashlib.sha256(merged).hexdigest()!=metadata['merged_sha256']:
        raise ValueError('固件与版本记录不匹配，已停止更新。')
    application=merged[0x10000:]
    if (not 24<=len(application)<=0x600000 or application[0]!=0xe9
            or int.from_bytes(application[12:14],'little')!=18):
        raise ValueError('应用镜像不是受支持的 ESP32-P4 固件。')
    if hashlib.sha256(application).hexdigest()!=metadata['components']['firmware.bin']:
        raise ValueError('应用程序校验失败，已停止更新。')
    if metadata['source_dirty']: raise ValueError('请先提交并重新构建固件，更新工具只接受已记录的版本。')
    return metadata,merged,application


def update(image,port=None,recovery=False):
    import serial
    from serial.tools import list_ports
    import esptool
    from esptool import cmds
    from esptool.targets.esp32p4 import ESP32P4ROM
    from esptool.loader import ESPLoader
    if esptool.__version__!='5.3.1': raise ValueError('本项目需要独立安装的 esptool 5.3.1。')
    metadata,merged,application=load_application(image)
    ports=list(list_ports.comports())
    native=[p.device for p in ports if p.vid==0x303a and p.pid==0x1001]
    if port is None:
        if len(native)!=1: raise ValueError('请用 USB2 数据线连接设备；需要唯一一台 ESP32 设备。')
        port=native[0]
    candidates=[port]+[p.device for p in ports if p.vid==0x1a86 and p.pid==0x55d3]
    phase=None
    for candidate in dict.fromkeys(candidates):
        try:
            with MacControl(candidate,reset_on_failure=False) as control:
                control.connect(timeout=2)
                candidate_phase=control.boot_status()['phase']
                if candidate_phase in ('emulator','stopped'): control.prepare_maintenance()
                phase=candidate_phase
                break
        except (ControlError,OSError): pass
    if phase=='emulator': raise ValueError('已准备维护。请在 10 分钟内先保存并在 Mac 的 Special 菜单正常关机，再运行更新。')
    if phase not in ('preboot','stopped') and not recovery:
        raise ValueError('无法确认系统已停止。若系统已经无法启动，可使用 --recovery 恢复更新。')
    folder=ROOT/'artifacts/updates'/datetime.now().strftime('%Y-%m-%d_%H%M%S_%f')
    folder.mkdir(parents=True)
    record={'image':image.name,'metadata':metadata,'port':port,'offset':0x10000,'verified':False}
    def save(): (folder/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    connection=serial.Serial(port=None,baudrate=115200,timeout=.5)
    connection.dtr=False;connection.rts=False;connection.port=port;connection.open()
    writing=False
    try:
        esp=ESP32P4ROM(connection);esp.connect(mode='usb-reset',attempts=3)
        if not esp.uses_usb_jtag_serial(): raise ValueError('更新必须通过内部 USB2 Serial/JTAG。')
        revision=esp.get_major_chip_version()*100+esp.get_minor_chip_version()
        record['mac']=':'.join(f'{b:02x}' for b in esp.read_mac())
        record['revision']=revision
        if not metadata['minimum_chip_revision']<=revision<=399: raise ValueError('固件不支持此芯片版本。')
        if metadata['chip_variant']=='esp32p4_es' and revision>=300:
            raise ValueError('生产芯片不能使用早期样片固件。')
        if esp.get_secure_boot_enabled() or esp.get_flash_encryption_enabled():
            raise ValueError('此更新流程不适用于已启用安全启动或加密的设备。')
        cmds.attach_flash(esp)
        if esp.flash_md5sum(0x8000,0x1000)!=hashlib.md5(merged[0x8000:0x9000]).hexdigest():
            raise ValueError('分区布局不同，不能使用保留数据的应用更新。')
        size=(len(application)+4095)//4096*4096
        original=bytearray()
        print('正在备份现有固件…',flush=True)
        # Prefer a byte-identical published image verified against the ROM's
        # own flash digest. This avoids P4 v3.2's unreliable streamed readback
        # and also makes the rollback version reproducible.
        candidates=list((ROOT/'release').glob('*.bin'))+list((ROOT/'release/archive').rglob('*.bin'))
        digests={}
        for candidate in sorted(candidates,key=lambda p:p.stat().st_mtime,reverse=True):
            try: old_meta,_,old_app=load_application(candidate)
            except (OSError,ValueError,KeyError): continue
            if old_meta.get('chip_variant')!=metadata['chip_variant']: continue
            if len(old_app) not in digests: digests[len(old_app)]=esp.flash_md5sum(0x10000,len(old_app))
            if digests[len(old_app)]==hashlib.md5(old_app).hexdigest():
                original=bytearray(old_app)
                record['backup_method']='Published application independently matched to current flash by ROM MD5'
                record['previous_release']=str(candidate.relative_to(ROOT))
                break
        if not original:
            esp=cmds.run_stub(esp);cmds.attach_flash(esp)
        for offset in range(0,size if not original else 0,0x1000):
            for attempt in range(3):
                try:
                    # P4 v3.2/native USB can lose a long streaming read. Keep
                    # each independently checked read within one flash page.
                    with contextlib.redirect_stdout(io.StringIO()):
                        data=cmds.read_flash(esp,0x10000+offset,min(0x1000,size-offset),no_progress=True)
                    if len(data)!=min(0x1000,size-offset): raise ValueError('备份读取不完整。')
                    break
                except (esptool.FatalError,OSError):
                    if attempt==2: raise
                    esp=ESP32P4ROM(connection);esp.connect(mode='usb-reset',attempts=2)
                    if record['mac']!=':'.join(f'{b:02x}' for b in esp.read_mac()): raise ValueError('设备身份改变。')
                    esp=cmds.run_stub(esp);cmds.attach_flash(esp)
            original.extend(data)
            if offset%0x40000==0: print(f'备份 {len(original)*100//size}%',flush=True)
        record.setdefault('backup_method','Paged flash read, followed by target digest verification')
        backup=folder/'previous-application.bin';backup.write_bytes(original)
        record['backup']=str(backup.relative_to(ROOT))
        record['backup_sha256']=hashlib.sha256(original).hexdigest();save()
        # Recheck the backup using the target's own flash digest before writing.
        cmds.verify_flash(esp,[(0x10000,bytes(original))])
        if not esp.IS_STUB: esp=cmds.run_stub(esp)
        print('备份通过校验，正在写入新固件…',flush=True)
        writing=True
        cmds.write_flash(esp,[(0x10000,application)],flash_mode='keep',flash_freq='keep',
                         flash_size='keep',compress=True,no_progress=True)
        # Verify independently through ROM, avoiding the streaming readback
        # failure seen on this P4 v3.2. SD, NVS and bootloader are untouched.
        connection.close();connection.open()
        esp=ESP32P4ROM(connection);esp.connect(mode='usb-reset',attempts=3)
        if record['mac']!=':'.join(f'{b:02x}' for b in esp.read_mac()): raise ValueError('设备身份改变。')
        cmds.attach_flash(esp);cmds.verify_flash(esp,[(0x10000,application)])
        record['verified']=True;record['application_sha256']=hashlib.sha256(application).hexdigest();save()
        print('更新与独立校验完成，正在重启。',flush=True)
        # Native RTS reset boots cleanly on this P4 v3.2. A watchdog reset
        # from download mode caused a first-boot bootloader fault/recovery.
        try: ESPLoader.hard_reset(esp,uses_usb=True)
        except (OSError,StopIteration): pass
        return folder
    except BaseException as error:
        record['error']=str(error)
        if not writing:
            try:
                esp=ESP32P4ROM(connection);esp.connect(mode='usb-reset',attempts=1)
                ESPLoader.hard_reset(esp,uses_usb=True)
            except Exception as reset_error: record['reset_note']=str(reset_error)
        save();raise
    finally: connection.close()


def main():
    parser=argparse.ArgumentParser(description='aMac USB2 固件更新（自动备份，保留 SD 和设置）')
    parser.add_argument('--port')
    parser.add_argument('--image',type=Path,default=ROOT/'release/aMac-P4C5-HD20034-Rev3.bin')
    parser.add_argument('--recovery',action='store_true',help='仅用于系统已经无法启动的恢复更新')
    args=parser.parse_args()
    try: print('记录：',update(args.image.resolve(),args.port,args.recovery));return 0
    except Exception as error:
        print('更新停止：',error,file=sys.stderr);return 1

if __name__=='__main__': raise SystemExit(main())
