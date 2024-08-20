# Stress Monitoring System

## 프로젝트 개요
이 프로젝트는 심박센서와 온습도 센서를 이용해 사용자의 스트레스 지수를 계산하고, 이를 라즈베리파이와 STM 보드를 통해 제어하는 시스템입니다. 스트레스 지수는 사용자의 심박수와 온습도 데이터를 바탕으로 계산되며, 3단계로 나누어 LED, LCD, 서보모터를 제어합니다.

## 시스템 아키텍처
1. **아두이노 (Arduino)**
   - **심박센서 (Heart Rate Sensor)**
   - **온습도센서 (Temperature & Humidity Sensor)**
   - 센서로부터 데이터를 수집한 후 WiFi를 통해 라즈베리파이로 전송합니다.

2. **라즈베리파이 (Raspberry Pi)**
   - 아두이노로부터 받은 데이터를 처리하여 MariaDB에 저장합니다.
   - 심박수와 온습도 데이터를 바탕으로 스트레스 지수를 계산합니다.
   - 계산된 스트레스 지수를 3단계로 구분하여 STM 보드로 전송합니다.
   - 3단계의 스트레스 지수에 따른 저장된 노래를 재생합니다.

3. **STM 보드 (STM32)**
   - 라즈베리파이로부터 받은 스트레스 지수를 기반으로 제어 신호를 생성합니다.
   - **LED**: 스트레스 단계에 따라 LED 색상을 변경합니다.
   - **LCD**: 현재 스트레스 상태를 표시합니다.
   - **서보모터**: 스트레스 단계에 따라 서보모터의 동작을 제어합니다.

## 플로우 차트
![image](https://github.com/user-attachments/assets/98e24ab8-5d50-4a2b-8f37-cb130d7879a0)

## 시현 영상
https://www.canva.com/design/DAGMBA3ns-M/mGyX5sD7rJ0MIVv8Kviplg/edit?utm_content=DAGMBA3ns-M&utm_campaign=designshare&utm_medium=link2&utm_source=sharebutton
