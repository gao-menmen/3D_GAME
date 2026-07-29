/**
 * 入口文件：初始化 Three.js 渲染器、摄像机、场景，
 * 驱动游戏主循环，协调各子系统
 */

import * as THREE from 'three';
import { Game } from './core/Game';
import { GameScene } from './world/Scene';
import { Player } from './player/Player';
import { Weapon } from './weapons/Weapon';
import { EnemyManager } from './enemies/Enemy';
import { HUD } from './ui/HUD';
import type { InputState } from './types';
import { CONFIG } from './types';

// ===================== 渲染器与摄像机 =====================

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
document.body.prepend(renderer.domElement);

const camera = new THREE.PerspectiveCamera(
  75,
  window.innerWidth / window.innerHeight,
  0.1,
  400,
);

// 摄像机需要先加入场景，武器才能挂到摄像机上并被渲染
const _cameraHolder = new THREE.Object3D();

// ===================== 子系统初始化 =====================

const gameState = new Game();
const gameScene = new GameScene();
const player = new Player(camera, renderer);
const weapon = new Weapon(camera);
const enemyManager = new EnemyManager(gameScene.scene);
const hud = new HUD();

// 将摄像机加入场景（武器已挂在摄像机上，Three.js 会一同渲染）
gameScene.scene.add(camera);
void _cameraHolder; // 抑制未使用警告

// PointerLockControls 需要加入场景才能正常工作
gameScene.scene.add(player.controls.getObject());

// ===================== 输入状态 =====================

const input: InputState = {
  forward: false,
  backward: false,
  left: false,
  right: false,
  jump: false,
  sprint: false,
  shooting: false,
  reload: false,
};

/** 按键映射 */
const KEY_MAP: Record<string, keyof InputState> = {
  KeyW: 'forward',
  KeyS: 'backward',
  KeyA: 'left',
  KeyD: 'right',
  Space: 'jump',
  ShiftLeft: 'sprint',
  ShiftRight: 'sprint',
  KeyR: 'reload',
};

window.addEventListener('keydown', (e) => {
  const key = KEY_MAP[e.code];
  if (key !== undefined) input[key] = true;
});

window.addEventListener('keyup', (e) => {
  const key = KEY_MAP[e.code];
  if (key !== undefined) input[key] = false;
});

window.addEventListener('mousedown', (e) => {
  if (e.button === 0 && gameState.isPlaying) input.shooting = true;
});

window.addEventListener('mouseup', (e) => {
  if (e.button === 0) input.shooting = false;
});

// ===================== 游戏状态管理 =====================

let killCount = 0;
let prevDamageFlash = false;

/** 开始 / 重新开始游戏 */
function startGame(): void {
  player.reset();
  weapon.reset();
  enemyManager.clear();
  killCount = 0;

  // 生成敌人
  enemyManager.spawn(CONFIG.enemyCount);

  gameState.startGame();
  hud.setState('playing');

  // 锁定鼠标
  player.controls.lock();
}

// HUD 按钮绑定
hud.onStart(() => startGame());
hud.onRestart(() => startGame());

// 初始显示菜单
hud.setState('menu');

// PointerLock 事件
player.controls.addEventListener('lock', () => {
  if (gameState.state === 'paused') {
    gameState.resume();
    hud.setState('playing');
  }
});

player.controls.addEventListener('unlock', () => {
  if (gameState.isPlaying) {
    gameState.pause();
    hud.setState('paused');
  }
});

// 暂停界面点击继续
const pauseScreen = document.getElementById('pause-screen');
pauseScreen?.addEventListener('click', () => {
  if (gameState.state === 'paused') {
    player.controls.lock();
  }
});

// ESC 在菜单/死亡界面不做额外处理（PointerLockControls 自动解锁）

// ===================== 窗口自适应 =====================

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// ===================== 主渲染循环 =====================

const clock = new THREE.Clock();

/** 当前帧玩家是否受伤 */
let playerHitThisFrame = false;

function loop(): void {
  requestAnimationFrame(loop);

  const dt = Math.min(clock.getDelta(), 0.05); // 最大 dt 限制防止卡顿穿墙

  if (gameState.isPlaying) {
    playerHitThisFrame = false;

    // 更新玩家（移动、重力、碰撞）
    player.update(dt, input, gameScene.obstacles);

    // 更新武器（冷却、换弹、闪光、后坐力）
    weapon.update(dt, input.shooting, input.reload);

    // 射击判定
    if (input.shooting) {
      const hit = weapon.shoot(
        gameScene.scene,
        enemyManager.getTargetMeshes(),
      );
      if (hit !== null) {
        const enemy = enemyManager.findByMesh(hit);
        if (enemy !== undefined) {
          enemy.takeDamage(CONFIG.bulletDamage);
        }
      }
    }

    // 更新敌人 AI
    const newKills = enemyManager.update(
      dt,
      camera.position,
      (damage) => {
        player.takeDamage(damage);
        playerHitThisFrame = true;
      },
      gameScene.obstacles,
    );
    killCount += newKills;

    // 受伤红屏
    if (playerHitThisFrame && !prevDamageFlash) {
      hud.triggerDamageFlash();
    }
    prevDamageFlash = playerHitThisFrame;

    // 更新 HUD
    hud.update(dt, {
      health: player.health,
      maxHealth: CONFIG.playerMaxHealth,
      magAmmo: weapon.magAmmo,
      reserveAmmo: weapon.reserveAmmo,
      reloading: weapon.reloading,
      kills: killCount,
    });

    // 玩家死亡检测
    if (player.isDead) {
      gameState.die();
      player.controls.unlock();
      hud.setState('dead', killCount);
    }

    // 所有敌人消灭后再生成一波
    if (enemyManager.enemies.length === 0 && !player.isDead) {
      enemyManager.spawn(CONFIG.enemyCount);
    }
  }

  // 渲染
  renderer.render(gameScene.scene, camera);
}

loop();
