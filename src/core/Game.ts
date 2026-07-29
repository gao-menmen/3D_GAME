/** 游戏状态机：管理 menu / playing / paused / dead 四种状态 */

import type { GameState } from '../types';

export class Game {
  private _state: GameState = 'menu';

  /** 当前游戏状态 */
  get state(): GameState {
    return this._state;
  }

  /** 是否正在游玩（非菜单、非暂停、非死亡） */
  get isPlaying(): boolean {
    return this._state === 'playing';
  }

  /** 切换到开始游戏状态 */
  startGame(): void {
    this._state = 'playing';
  }

  /** 切换到暂停状态 */
  pause(): void {
    if (this._state === 'playing') {
      this._state = 'paused';
    }
  }

  /** 从暂停恢复游戏 */
  resume(): void {
    if (this._state === 'paused') {
      this._state = 'playing';
    }
  }

  /** 玩家死亡 */
  die(): void {
    this._state = 'dead';
  }

  /** 重新开始游戏（返回 playing 状态，由外部重置游戏数据） */
  restart(): void {
    this._state = 'playing';
  }
}
