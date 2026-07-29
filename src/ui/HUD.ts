/**
 * HUD 与菜单界面更新逻辑
 * 负责：准星、血量、弹药、击杀计数、受伤红屏反馈、菜单/暂停/死亡界面显示控制
 */

import type { GameState, HudData } from '../types';

/** DOM 元素引用缓存 */
interface DomRefs {
  hud: HTMLElement;
  healthFill: HTMLElement;
  healthValue: HTMLElement;
  ammoMag: HTMLElement;
  ammoReserve: HTMLElement;
  reloadHint: HTMLElement;
  killsValue: HTMLElement;
  damageFlash: HTMLElement;
  menu: HTMLElement;
  startBtn: HTMLButtonElement;
  pauseScreen: HTMLElement;
  deadScreen: HTMLElement;
  deadKills: HTMLElement;
  restartBtn: HTMLButtonElement;
}

export class HUD {
  private readonly _dom: DomRefs;

  /** 受伤红屏消退计时 */
  private _flashTimer: number = 0;

  constructor() {
    this._dom = this._queryDom();
  }

  /** 从 DOM 查找并缓存所有元素引用 */
  private _queryDom(): DomRefs {
    const q = <T extends HTMLElement>(sel: string): T => {
      const el = document.querySelector<T>(sel);
      if (el === null) throw new Error(`HUD element not found: ${sel}`);
      return el;
    };

    return {
      hud: q('#hud'),
      healthFill: q('#health-fill'),
      healthValue: q('#health-value'),
      ammoMag: q('#ammo-mag'),
      ammoReserve: q('#ammo-reserve'),
      reloadHint: q('#reload-hint'),
      killsValue: q('#kills-value'),
      damageFlash: q('#damage-flash'),
      menu: q('#menu'),
      startBtn: q<HTMLButtonElement>('#start-btn'),
      pauseScreen: q('#pause-screen'),
      deadScreen: q('#dead-screen'),
      deadKills: q('#dead-kills'),
      restartBtn: q<HTMLButtonElement>('#restart-btn'),
    };
  }

  /** 每帧更新 HUD 数据显示 */
  update(dt: number, data: HudData): void {
    // 血量条
    const hpPct = Math.max(0, (data.health / data.maxHealth) * 100);
    this._dom.healthFill.style.width = `${hpPct.toFixed(1)}%`;
    this._dom.healthValue.textContent = String(Math.ceil(data.health));

    // 根据血量改变颜色
    if (hpPct > 60) {
      this._dom.healthFill.style.background = 'linear-gradient(90deg, #e74c3c, #e67e22)';
    } else if (hpPct > 25) {
      this._dom.healthFill.style.background = 'linear-gradient(90deg, #c0392b, #e74c3c)';
    } else {
      this._dom.healthFill.style.background = '#c0392b';
    }

    // 弹药
    this._dom.ammoMag.textContent = String(data.magAmmo);
    this._dom.ammoReserve.textContent = String(data.reserveAmmo);
    this._dom.reloadHint.style.display = data.reloading ? 'block' : 'none';

    // 击杀
    this._dom.killsValue.textContent = String(data.kills);

    // 受伤红屏消退
    if (this._flashTimer > 0) {
      this._flashTimer -= dt;
      const alpha = Math.max(0, this._flashTimer / 0.4);
      this._dom.damageFlash.style.background = `rgba(220, 20, 20, ${alpha * 0.45})`;
    }
  }

  /** 触发受伤红屏闪烁 */
  triggerDamageFlash(): void {
    this._flashTimer = 0.4;
    this._dom.damageFlash.style.background = 'rgba(220, 20, 20, 0.45)';
  }

  /** 根据游戏状态切换界面层显示 */
  setState(state: GameState, kills?: number): void {
    switch (state) {
      case 'menu':
        this._dom.hud.style.display = 'none';
        this._dom.menu.style.display = 'flex';
        this._dom.pauseScreen.style.display = 'none';
        this._dom.deadScreen.style.display = 'none';
        break;

      case 'playing':
        this._dom.hud.style.display = 'block';
        this._dom.menu.style.display = 'none';
        this._dom.pauseScreen.style.display = 'none';
        this._dom.deadScreen.style.display = 'none';
        break;

      case 'paused':
        this._dom.hud.style.display = 'block';
        this._dom.menu.style.display = 'none';
        this._dom.pauseScreen.style.display = 'flex';
        this._dom.deadScreen.style.display = 'none';
        break;

      case 'dead':
        this._dom.hud.style.display = 'none';
        this._dom.menu.style.display = 'none';
        this._dom.pauseScreen.style.display = 'none';
        this._dom.deadScreen.style.display = 'flex';
        this._dom.deadKills.textContent = `本局击杀：${kills ?? 0}`;
        break;
    }
  }

  /** 注册"开始游戏"按钮回调 */
  onStart(cb: () => void): void {
    this._dom.startBtn.addEventListener('click', cb);
  }

  /** 注册"重新开始"按钮回调 */
  onRestart(cb: () => void): void {
    this._dom.restartBtn.addEventListener('click', cb);
  }
}
