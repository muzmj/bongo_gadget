# bongo_pc_improved.py
import os, sys, json, threading, time
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import simpledialog, messagebox
import pystray
from PIL import Image
import keyboard

CONFIG_FILE = "config.json"
ICON_FILE = "bongo_middle.png"  # 같은 폴더에 두거나 --add-data로 포함

config = {"com_port": "", "wifi_ssid": "", "wifi_pass": ""}

ser = None
ser_lock = threading.Lock()

def resource_path(rel):
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, rel)
    return os.path.join(os.path.abspath("."), rel)

def load_config():
    global config
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                config.update(json.load(f))
        except Exception as e:
            print(f"Config 로드 실패: {e}")

def save_config():
    try:
        with open(CONFIG_FILE, "w", encoding="utf-8") as f:
            json.dump(config, f, ensure_ascii=False, indent=2)
        print("Config 저장 완료")
    except Exception as e:
        print(f"Config 저장 실패: {e}")

# ------------ UI (Tk) for setup ------------
def ask_setup():
    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    
    # COM 포트 선택
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if not ports:
        messagebox.showerror("에러", "사용 가능한 COM 포트가 없습니다.")
        root.destroy()
        return False
    
    port_str = ", ".join(ports)
    com = simpledialog.askstring(
        "COM 포트", 
        f"사용할 COM 포트 선택 (예: COM3)\n\n사용 가능한 포트:\n{port_str}", 
        initialvalue=config.get("com_port", ports[0] if ports else "")
    )
    if com is None:
        root.destroy()
        return False
    
    # SSID 입력
    ssid = simpledialog.askstring(
        "Wi-Fi SSID", 
        "Wi-Fi 네트워크 이름(SSID)을 입력하세요:", 
        initialvalue=config.get("wifi_ssid", "")
    )
    if ssid is None:
        root.destroy()
        return False
    
    # 비밀번호 입력
    pwd = simpledialog.askstring(
        "Wi-Fi Password", 
        f"'{ssid}' 네트워크의 비밀번호를 입력하세요:", 
        show="*", 
        initialvalue=config.get("wifi_pass", "")
    )
    if pwd is None:
        root.destroy()
        return False

    # 입력값 검증 및 저장
    config["com_port"] = com.strip()
    config["wifi_ssid"] = ssid.strip()
    config["wifi_pass"] = pwd.strip()
    
    # 빈 값 체크
    if not config["com_port"] or not config["wifi_ssid"]:
        messagebox.showerror("에러", "COM 포트와 SSID는 필수입니다.")
        root.destroy()
        return False
    
    save_config()
    print(f"설정 저장됨 - COM: {config['com_port']}, SSID: {config['wifi_ssid']}")
    
    root.destroy()
    return True

# ------------ Serial open/send ------------
def open_serial():
    global ser
    if not config.get("com_port"):
        print("COM 포트가 설정되지 않았습니다.")
        return False
    
    try:
        with ser_lock:
            if ser and ser.is_open:
                ser.close()
                time.sleep(0.5)  # 포트 해제 대기
            
            print(f"시리얼 포트 열기 시도: {config['com_port']}")
            ser = serial.Serial(config["com_port"], 115200, timeout=1)
            
            # ESP8266 리셋 후 안정화 대기
            ser.setDTR(False)
            ser.setRTS(False)
            time.sleep(2)  # ESP8266 부팅 완료 대기
            
            # 버퍼 비우기
            ser.flushInput()
            ser.flushOutput()
            
        print(f"시리얼 포트 {config['com_port']} 열기 성공")
        return True
        
    except Exception as e:
        print(f"시리얼 포트 열기 실패: {e}")
        return False

def send_wifi_to_device():
    global ser
    if not config.get("wifi_ssid"):
        print("Wi-Fi SSID가 설정되지 않았습니다.")
        return False
    
    # Wi-Fi 설정 명령 생성
    cmd = f"WIFI:{config['wifi_ssid']},{config['wifi_pass']}\n"
    
    try:
        with ser_lock:
            if ser and ser.is_open:
                print(f"Wi-Fi 설정 전송: SSID='{config['wifi_ssid']}'")
                
                # 여러 번 전송해서 확실하게 받도록
                for i in range(3):
                    ser.write(cmd.encode('utf-8'))
                    time.sleep(0.1)
                
                ser.flush()  # 전송 완료 대기
                print("Wi-Fi 설정 전송 완료")
                return True
            else:
                print("시리얼 포트가 열려있지 않습니다.")
                return False
                
    except Exception as e:
        print(f"Wi-Fi 설정 전송 실패: {e}")
        return False

def send_key():
    try:
        with ser_lock:
            if ser and ser.is_open:
                ser.write(b"KEY\n")
                ser.flush()
            else:
                print("시리얼 포트가 열려있지 않습니다.")
    except Exception as e:
        print(f"키 전송 실패: {e}")

def test_connection():
    """연결 테스트 함수"""
    try:
        with ser_lock:
            if ser and ser.is_open:
                ser.write(b"TEST\n")
                ser.flush()
                print("테스트 명령 전송")
                return True
            else:
                print("시리얼 포트가 열려있지 않습니다.")
                return False
    except Exception as e:
        print(f"연결 테스트 실패: {e}")
        return False

# ------------ Keyboard listener (background) ------------
def keyboard_loop():
    def on_event(e):
        if e.event_type == "down":
            send_key()
    
    try:
        keyboard.hook(on_event)
        print("키보드 모니터링 시작")
        keyboard.wait()
    except Exception as e:
        print(f"키보드 후킹 실패: {e}")

# ------------ Tray and menu ------------
def show_settings_from_tray(icon=None, item=None):
    def _runner():
        ok = ask_setup()
        if ok:
            reopen_serial_and_apply()
    threading.Thread(target=_runner, daemon=True).start()

def test_connection_from_tray(icon=None, item=None):
    def _runner():
        if test_connection():
            messagebox.showinfo("연결 테스트", "ESP8266과의 연결이 정상입니다.")
        else:
            messagebox.showerror("연결 테스트", "ESP8266과의 연결에 문제가 있습니다.")
    threading.Thread(target=_runner, daemon=True).start()

def send_wifi_from_tray(icon=None, item=None):
    def _runner():
        if send_wifi_to_device():
            messagebox.showinfo("Wi-Fi 설정", "Wi-Fi 설정을 ESP8266에 전송했습니다.")
        else:
            messagebox.showerror("Wi-Fi 설정", "Wi-Fi 설정 전송에 실패했습니다.")
    threading.Thread(target=_runner, daemon=True).start()

def reopen_serial_and_apply():
    global ser
    print("시리얼 포트 재연결 시도...")
    
    with ser_lock:
        try:
            if ser and ser.is_open:
                ser.close()
        except:
            pass
    
    time.sleep(1)  # 충분한 대기 시간
    
    success = open_serial()
    if success:
        time.sleep(1)  # ESP8266 준비 대기
        wifi_success = send_wifi_to_device()
        
        if wifi_success:
            messagebox.showinfo("성공", "시리얼 연결 및 Wi-Fi 설정이 완료되었습니다.")
        else:
            messagebox.showwarning("경고", "시리얼 연결은 성공했지만 Wi-Fi 설정 전송에 실패했습니다.")
    else:
        messagebox.showerror("에러", f"시리얼 포트 {config.get('com_port')} 열기에 실패했습니다.")

def stop_app(icon=None, item=None):
    print("프로그램 종료")
    try:
        with ser_lock:
            if ser and ser.is_open:
                ser.close()
    except:
        pass
    icon.stop()
    os._exit(0)

def run_tray():
    try:
        image = Image.open(resource_path(ICON_FILE))
    except:
        # 아이콘 파일이 없는 경우 기본 아이콘 생성
        image = Image.new('RGB', (64, 64), color='blue')
    
    menu = pystray.Menu(
        pystray.MenuItem("설정", show_settings_from_tray),
        pystray.MenuItem("Wi-Fi 재전송", send_wifi_from_tray),
        pystray.MenuItem("연결 테스트", test_connection_from_tray),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem("종료", stop_app)
    )
    icon = pystray.Icon("BongoCat", image, "BongoCat Controller", menu)
    print("시스템 트레이 실행")
    icon.run()

# ------------ Main ------------
def main():
    print("BongoCat Controller 시작")
    load_config()
    
    # 초기 설정 확인
    if not config.get("com_port") or not config.get("wifi_ssid"):
        print("초기 설정이 필요합니다.")
        ok = ask_setup()
        if not ok:
            print("초기 설정이 취소되어 종료합니다.")
            return

    # 시리얼 포트 열기 및 Wi-Fi 설정 전송
    print("시리얼 포트 연결 시도...")
    if not open_serial():
        messagebox.showerror("에러", f"시리얼 포트 {config.get('com_port')} 열기 실패")
        return
    
    # Wi-Fi 설정 전송
    print("Wi-Fi 설정 전송...")
    time.sleep(1)  # ESP8266 준비 대기
    if not send_wifi_to_device():
        messagebox.showwarning("경고", "Wi-Fi 설정 전송 실패. 나중에 트레이 메뉴에서 재시도하세요.")

    # 키보드 모니터링 시작
    kb_thread = threading.Thread(target=keyboard_loop, daemon=True)
    kb_thread.start()

    print("모든 초기화 완료. 시스템 트레이에서 실행 중...")
    
    # 트레이 실행 (메인 스레드에서 블로킹)
    run_tray()

if __name__ == "__main__":
    main()